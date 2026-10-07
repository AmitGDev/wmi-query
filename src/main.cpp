/*
    main.cpp
    Copyright (c) 2024-2026, Amit Gefen

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <exception>
#include <expected>
#include <format>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "Formatters.hpp"
#include "WmiClient.hpp"
#include "WmiNamespace.hpp"
#include "WmiWrapper.hpp"
#include "scope.hpp"

namespace {

// **** For MultiQueryDrives example ****

// Example of a multi-query class: it chains three queries (drives, then each
// drive's partitions, then each partition's logical drives) over one
// connection and reuses WmiNamespace's static helpers. It does not derive from
// WmiNamespace because its result is not a single table.
class Drives final {
 public:
  // A physical drive and its associated logical drives (volumes).
  using PhysicalDriveEntry = std::pair<amitgdev::Column, amitgdev::Columns>;

  [[nodiscard]] const std::vector<PhysicalDriveEntry>& Data() const noexcept {
    return data_;
  }

  // Sum of all physical drive sizes, in bytes.
  // A drive whose size cannot be read counts as 0.
  [[nodiscard]] std::uint64_t GetTotalSize() const noexcept {
    std::uint64_t total_size{0};

    for (const auto& drive_entry : data_) {
      try {
        const auto size =
            amitgdev::WmiNamespace::GetValue(drive_entry.first, L"Size");
        if (!size) {
          continue;
        }

        total_size += std::wcstoull(size->c_str(), nullptr, 10);
      } catch (...) {  // NOLINT(bugprone-empty-catch)
        // Treat an unreadable drive as having size zero.
      }
    }

    return total_size;
  }

  // Returns the number of physical drives found, or the HRESULT of the first
  // failed query.
  [[nodiscard]] std::expected<size_t, amitgdev::Result> Query() {
    // Cleared first so any failure below leaves data_ empty, never stale.
    data_.clear();

    auto client = amitgdev::WmiClient::Create(L"ROOT\\CIMV2");
    if (!client) {
      return std::unexpected(client.error());
    }

    static const amitgdev::Rows physical_drive_rows{
        L"DeviceId",     L"Caption", L"Model",
        L"SerialNumber", L"Size",    L"InterfaceType",
    };

    // Query 1 of 3: Query physical drives.
    auto physical_drives =
        client->Query(L"SELECT * FROM Win32_DiskDrive", physical_drive_rows);
    if (!physical_drives) {
      return std::unexpected(physical_drives.error());
    }

    // Build locally and commit at the end, so a failure halfway through
    // never leaves data_ holding a partial drive list.
    decltype(data_) data;
    data.reserve(physical_drives->size());

    for (auto& physical_drive : *physical_drives) {
      const std::wstring physical_drive_id =
          amitgdev::WmiNamespace::GetValue(physical_drive, L"DeviceId")
              .value_or(std::wstring{});

      auto logical_drives = QueryLogicalDrives(*client, physical_drive_id);
      if (!logical_drives) {
        return std::unexpected(logical_drives.error());
      }

      data.emplace_back(std::move(physical_drive), std::move(*logical_drives));
    }

    data_ = std::move(data);
    return data_.size();
  }

 private:
  // Walks drive -> partitions -> logical drives. Not every partition has an
  // associated Win32_LogicalDisk instance. All query results are processed,
  // and query failures are propagated rather than treated as missing drives.
  [[nodiscard]] static std::expected<amitgdev::Columns, amitgdev::Result>
  QueryLogicalDrives(amitgdev::WmiClient& client,
                     const std::wstring& physical_drive_id) {
    amitgdev::Columns logical_drives;

    if (physical_drive_id.empty()) {
      return logical_drives;
    }

    static const amitgdev::Rows logical_drive_rows{
        L"Caption", L"VolumeName", L"FileSystem",
        L"Size",    L"FreeSpace",  L"VolumeSerialNumber",
    };

    // Query 2 of 3: Get the partitions on this physical drive.
    const auto partitions = client.Query(
        L"ASSOCIATORS OF {Win32_DiskDrive.DeviceID='" + physical_drive_id +
            L"'} WHERE AssocClass=Win32_DiskDriveToDiskPartition",
        {L"DeviceId"});
    if (!partitions) {
      return std::unexpected(partitions.error());
    }

    for (const auto& partition : *partitions) {
      const auto partition_id =
          amitgdev::WmiNamespace::GetValue(partition, L"DeviceId");

      // A partition without a readable id cannot be looked up, so it is
      // skipped.
      if (!partition_id || partition_id->empty()) {
        continue;
      }

      // Query 3 of 3: Get the logical drives on this partition.
      auto partition_logical_drives = client.Query(
          L"ASSOCIATORS OF {Win32_DiskPartition.DeviceID='" + *partition_id +
              L"'} WHERE AssocClass=Win32_LogicalDiskToPartition",
          logical_drive_rows);
      if (!partition_logical_drives) {
        return std::unexpected(partition_logical_drives.error());
      }

      for (auto& logical_drive : *partition_logical_drives) {
        logical_drives.emplace_back(std::move(logical_drive));
      }
    }

    return logical_drives;
  }

  std::vector<PhysicalDriveEntry> data_;
};

}  // namespace

// **** Formatter for Processor example ****

// Intent: follow SMBIOS Type 4 literally. Bit 7 set means bits 0-6 hold the
// voltage in tenths of a volt. Bit 7 clear means the byte is a capability
// mask where only bits 0-2 are defined; reserved bits (3-6) are never
// interpreted, so firmware junk can't be mistaken for a voltage.
static std::wstring FormatCurrentVoltage(const std::wstring& current_voltage) {
  struct VoltageCapability {
    std::uint8_t bit;
    std::wstring_view label;
  };

  constexpr std::array<VoltageCapability, 3> kCapabilities{
      VoltageCapability{.bit = 0x01, .label = L"5V"},
      VoltageCapability{.bit = 0x02, .label = L"3.3V"},
      VoltageCapability{.bit = 0x04, .label = L"2.9V"},
  };
  constexpr std::uint8_t kTenthsFlag{0x80};
  constexpr std::uint8_t kValueMask{0x7F};
  constexpr std::uint8_t kCapabilityMask{0x07};
  // A view (not a const std::wstring) so returning it never blocks a move.
  constexpr std::wstring_view kUnavailable{L"N/A"};

  try {
    const auto parsed = std::stoull(current_voltage);
    if (parsed > std::numeric_limits<std::uint8_t>::max()) {
      return std::wstring{kUnavailable};
    }

    const auto value = static_cast<std::uint8_t>(parsed);

    if ((value & kTenthsFlag) != 0) {
      const auto tenths = static_cast<std::uint8_t>(value & kValueMask);
      return std::to_wstring(tenths / 10) + L"." +
             std::to_wstring(tenths % 10) + L"V";
    }

    // Any bit outside the defined capability range means malformed data.
    const auto defined = static_cast<std::uint8_t>(value & kCapabilityMask);
    if (defined != value) {
      return std::wstring{kUnavailable};
    }

    std::wstring supported;
    for (const VoltageCapability& capability : kCapabilities) {
      if ((value & capability.bit) == 0) {
        continue;
      }
      if (!supported.empty()) {
        supported += L", ";
      }
      supported += capability.label;
    }

    if (supported.empty()) {
      return std::wstring{kUnavailable};
    }
    return L"Supports " + supported;
  } catch (const std::exception&) {
    return std::wstring{kUnavailable};
  }
}

// **** Formatter for Memory example ****

// A formatter is a plain function from text to text, independent of WMI.
// Pass by value intentionally: preserve the original value for a lossless
// fallback if anything goes wrong.
static std::wstring
ConvertFormFactorCodeToName(std::wstring form_factor_code) noexcept {
  static constexpr std::array<std::wstring_view, 24> kFormFactorNameLut{
      L"Unknown",     L"Other", L"SIP",  L"DIP",  L"ZIP",   L"SOJ",
      L"Proprietary", L"SIMM",  L"DIMM", L"TSOP", L"PGA",   L"RIMM",
      L"SODIMM",      L"SRIMM", L"SMD",  L"SSMP", L"QFP",   L"TQFP",
      L"SOIC",        L"LCC",   L"PLCC", L"BGA",  L"FPBGA", L"LGA",
  };

  try {
    // Exceptions: std::invalid_argument if no conversion could be performed.
    // std::out_of_range if the converted value would fall out of the range
    // of the result type or if it sets errno to ERANGE.
    if (const auto code = std::stoul(form_factor_code);
        code < kFormFactorNameLut.size()) {
      return std::wstring(kFormFactorNameLut.at(code));
    }
  } catch (...) {  // NOLINT(bugprone-empty-catch)
  }

  return form_factor_code;
}

// **** Print functions ****

// Abstractly print WmiNamespace data.
static void Print(const amitgdev::Columns& data) {
  for (const auto& column : data) {
    for (const auto& [row, value] : column) {
      std::wcout << L" " << row << L" : ";

      // get_if instead of get: a value of an unexpected alternative must not
      // throw out of a diagnostic printer.
      if (const auto* text = std::get_if<std::wstring>(&value)) {
        std::wcout << *text;
      } else if (const auto* number = std::get_if<int>(&value)) {
        std::wcout << *number;
      }

      std::wcout << L'\n';
    }
    std::wcout << L'\n';
  }

  std::wcout << std::flush;
}

// Print each physical drive, then its logical drives, with the generic Print.
// Sizes are shown in binary units. The formatting is done on copies, because
// the cached result is read-only.
static void PrintDrives(const std::vector<Drives::PhysicalDriveEntry>& data) {
  for (const auto& [physical_drive, partitions] : data) {
    // Physical drive:
    const amitgdev::Columns columns{physical_drive};
    Print(columns);

    // Partitions:
    Print(partitions);
  }
}

// A failed query must not be reported as "0 objects, empty table", so each
// example stops at the first failure and says why.
static bool Check(const std::expected<std::size_t, amitgdev::Result>& result) {
  if (result) {
    return true;
  }

  std::wcerr << std::format(L" Query failed: HRESULT {:#010x}\n\n",
                            static_cast<std::uint32_t>(result.error()));
  return false;
}

// **** EXAMPLES: ****

// Namespace example
static void TimeZone() {
  std::wcout << L"Time Zone:\n\n";

  amitgdev::WmiNamespace time_zone(L"Win32_TimeZone",
                                   {L"Bias", L"DaylightName", L"Description"});

  if (!Check(time_zone.Query())) {
    return;
  }
  Print(time_zone.Data());  // Abstract print of the result.
}

// Namespace example
static void Motherboard() {
  std::wcout << L"Motherboard:\n\n";

  amitgdev::WmiNamespace motherboard(
      L"Win32_BaseBoard",
      {L"Caption", L"Manufacturer", L"Product", L"SerialNumber"});

  if (!Check(motherboard.Query())) {
    return;
  }
  Print(motherboard.Data());
}

// Namespace with formatter example.
static void Bios() {
  std::wcout << L"BIOS:\n\n";

  amitgdev::WmiNamespace bios(
      L"Win32_BIOS",
      {
          L"Version",
          L"ReleaseDate",
          L"SerialNumber",
      },
      {
          {.row_title = L"ReleaseDate", .format = amitgdev::FormatDate},
      });

  if (!Check(bios.Query())) {
    return;
  }
  Print(bios.Data());
}

// Namespace with formatter example.
static void Processor() {
  std::wcout << L"CPU:\n\n";

  amitgdev::WmiNamespace cpu(
      L"Win32_Processor",
      {
          L"Caption",
          L"NumberOfCores",
          L"MaxClockSpeed",
          L"L2CacheSize",
          L"DataWidth",
          L"Manufacturer",
          L"Name",
          L"SocketDesignation",
          L"CurrentVoltage",
      },
      {
          {.row_title = L"CurrentVoltage", .format = FormatCurrentVoltage},
      });

  if (!Check(cpu.Query())) {
    return;
  }
  Print(cpu.Data());
}

// Namespace with 2 formatters example.
static void Memory() {
  std::wcout << L"Memory:\n\n";

  amitgdev::WmiNamespace physical_memory(
      L"Win32_PhysicalMemory",
      {
          L"Caption",
          L"BankLabel",
          L"Capacity",
          L"FormFactor",
          L"Manufacturer",
          L"SerialNumber",
      },
      {
          {.row_title = L"Capacity", .format = amitgdev::FormatB},
          {.row_title = L"FormFactor", .format = ConvertFormFactorCodeToName},
      });

  if (!Check(physical_memory.Query())) {
    return;
  }
  Print(physical_memory.Data());
}

// Namespace with formatter example.
static void Gpu() {
  std::wcout << L"GPU:\n\n";

  amitgdev::WmiNamespace gpu(
      L"Win32_VideoController",
      {
          L"Caption",
          L"Name",
          L"AdapterRAM",
          L"DriverVersion",
          L"DriverDate",
      },
      {
          {.row_title = L"DriverDate", .format = amitgdev::FormatDate},
      });

  if (!Check(gpu.Query())) {
    return;
  }
  Print(gpu.Data());
}

static void SerialPorts() {
  std::wcout << L"Serial Ports:\n\n";

  amitgdev::WmiNamespace serial_ports(L"Win32_SerialPort",
                                      {L"Description", L"DeviceID"});

  if (!Check(serial_ports.Query())) {
    return;
  }
  Print(serial_ports.Data());
}

// Multi-query example.
static void MultiQueryDrives() {
  std::wcout << L"Physical & Logical Drives:\n\n";

  Drives drives;

  if (!Check(drives.Query())) {
    return;
  }

  PrintDrives(drives.Data());
  std::wcout << L" total size: "
             << amitgdev::FormatB(std::to_wstring(drives.GetTotalSize()))
             << L"\n\n";
}

int main() {
  // COM must be initialized before issuing WMI queries.
  // Fail explicitly rather than producing an empty result.
  if (amitgdev::WmiWrapper::Initialize() < 0) {
    return EXIT_FAILURE;
  }

  const amitgdev::scope_exit cleanup_guard{
      [] noexcept { amitgdev::WmiWrapper::Uninitialize(); }};

  try {
    // System Context
    TimeZone();

    // Platform and firmware
    Motherboard();
    Bios();
    Processor();
    Memory();

    // Devices
    Gpu();
    SerialPorts();
    MultiQueryDrives();
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    // fputs instead of std::println: the error path must not throw again.
    std::fputs(e.what(), stderr);
    std::fputs("\n", stderr);
    return EXIT_FAILURE;
  }
}
