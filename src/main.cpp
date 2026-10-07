// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026, Amit Gefen

#include <winerror.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <exception>
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
  // A physical drive and one or more of its volumes (logical drives).
  using DriveWithVolumes = std::pair<amitgdev::Column, amitgdev::Columns>;

  [[nodiscard]] const std::vector<DriveWithVolumes>& Data() const noexcept {
    return data_;
  }

  // Sum of all physical drive sizes, in bytes.
  // A drive whose size cannot be read counts as 0.
  [[nodiscard]] std::uint64_t GetTotalSize() const noexcept {
    std::uint64_t total_size{0};

    for (const auto& drive_entry : data_) {
      try {
        const std::wstring size = ValueAt(drive_entry.first, L"Size");
        total_size += std::wcstoull(size.c_str(), nullptr, 10);
      } catch (...) {  // NOLINT(bugprone-empty-catch)
      }
    }

    return total_size;
  }

  // Value of row_title in one column (a drive or a logical drive).
  [[nodiscard]] static std::wstring ValueAt(const amitgdev::Column& column,
                                            const std::wstring& row_title) {
    const size_t row_index =
        amitgdev::WmiNamespace::GetRowIndex(column, row_title);
    if (row_index == SIZE_MAX) {
      return {};
    }

    const auto* text = std::get_if<std::wstring>(&column.at(row_index).second);

    return text != nullptr ? *text : std::wstring{};
  }

  // Returns the number of physical drives found.
  size_t Query() {
    // One connection serves every query instead of one connection per query.
    amitgdev::WmiClient client(L"ROOT\\CIMV2");

    const amitgdev::Rows drive_rows{
        L"DeviceId",     L"Caption", L"Model",
        L"SerialNumber", L"Size",    L"InterfaceType",
    };

    data_.clear();

    // Query 1 of 3: Query physical drives.
    for (auto& physical_drive :
         client.Query(L"SELECT * FROM Win32_DiskDrive", drive_rows)) {
      // Query partitions and logical drives.
      amitgdev::Columns logical_drives =
          QueryLogicalDrives(client, ValueAt(physical_drive, L"DeviceId"));
      data_.emplace_back(std::move(physical_drive), std::move(logical_drives));
    }

    return data_.size();
  }

 private:
  // Walks drive -> partitions -> logical drives. A partition without a drive
  // letter has no logical drive, so it does not appear in the result.
  static amitgdev::Columns QueryLogicalDrives(amitgdev::WmiClient& client,
                                              const std::wstring& drive_id) {
    amitgdev::Columns logical_drives;

    if (drive_id.empty()) {
      return logical_drives;
    }

    const amitgdev::Rows volume_rows{
        L"Caption", L"VolumeName", L"FileSystem",
        L"Size",    L"FreeSpace",  L"VolumeSerialNumber",
    };

    // Query 2 of 3: Get the partitions on this physical drive.
    const amitgdev::Columns partitions =
        client.Query(L"ASSOCIATORS OF {Win32_DiskDrive.DeviceID='" + drive_id +
                         L"'} WHERE AssocClass=Win32_DiskDriveToDiskPartition",
                     {L"DeviceId"});

    for (const auto& partition : partitions) {
      const std::wstring partition_id = ValueAt(partition, L"DeviceId");
      if (partition_id.empty()) {
        continue;
      }

      // Query 3 of 3: Get the logical drives on this partition.
      for (auto& volume :
           client.Query(L"ASSOCIATORS OF {Win32_DiskPartition.DeviceID='" +
                            partition_id +
                            L"'} WHERE AssocClass=Win32_LogicalDiskToPartition",
                        volume_rows)) {
        logical_drives.emplace_back(std::move(volume));
      }
    }

    return logical_drives;
  }

  std::vector<DriveWithVolumes> data_;
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
}

// Print each physical drive, then its logical drives, with the generic Print.
// Sizes are shown in binary units. The formatting is done on copies, because
// the cached result is read-only.
static void PrintDrives(const std::vector<Drives::DriveWithVolumes>& data) {
  for (const auto& [physical_drive, partitions] : data) {
    // Physical drive:
    const amitgdev::Columns columns{physical_drive};
    Print(columns);

    // Partitions:
    Print(partitions);
  }
}

// **** EXAMPLES: ****

// Namespace example
static void TimeZone() {
  std::wcout << L"Time Zone:\n\n";

  amitgdev::WmiNamespace time_zone(L"Win32_TimeZone",
                                   {L"Bias", L"DaylightName", L"Description"});

  time_zone.Query();
  Print(time_zone.Data());  // Abstract print of the result.
}

// Namespace example
static void Motheboard() {
  std::wcout << L"Motherboard:\n\n";

  amitgdev::WmiNamespace motherboard(
      L"Win32_BaseBoard",
      {L"Caption", L"Manufacturer", L"Product", L"SerialNumber"});

  motherboard.Query();
  Print(motherboard.Data());
}

static void Bios() {
  std::wcout << L"BIOS:\n\n";

  amitgdev::WmiNamespace bios(L"Win32_BIOS",
                              {L"Version", L"ReleaseDate", L"SerialNumber"});

  bios.Query();
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

  cpu.Query();
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

  physical_memory.Query();
  Print(physical_memory.Data());
}

// Namespace example
static void Gpu() {
  std::wcout << L"GPU:\n\n";

  amitgdev::WmiNamespace gpu(
      L"Win32_VideoController",
      {L"Caption", L"Name", L"AdapterRAM", L"DriverVersion", L"DriverDate"});

  gpu.Query();
  Print(gpu.Data());
}

static void SerialPorts() {
  std::wcout << L"Serial Ports:\n\n";

  amitgdev::WmiNamespace serial_ports(L"Win32_SerialPort",
                                      {L"Description", L"DeviceID"});

  serial_ports.Query();
  Print(serial_ports.Data());
}

// Multi-query example.
static void MultiQueryDrives() {
  std::wcout << L"Physical & Logical Drives:\n\n";

  Drives drives;

  drives.Query();
  PrintDrives(drives.Data());
  std::wcout << L" total size: "
             << amitgdev::FormatB(std::to_wstring(drives.GetTotalSize()))
             << L"\n\n";
}

int main() {
  // COM must be initialized before issuing WMI queries.
  // Fail explicitly rather than producing an empty result.
  if (FAILED(amitgdev::WmiWrapper::Initialize())) {
    return EXIT_FAILURE;
  }

  const amitgdev::scope_exit cleanup_guard{
      [] noexcept { amitgdev::WmiWrapper::Uninitialize(); }};

  try {
    // System Context
    TimeZone();

    // Platform and firmware
    Motheboard();
    Bios();
    Processor();
    Memory();

    // Devices
    Gpu();
    SerialPorts();
    MultiQueryDrives();
    return EXIT_SUCCESS;
  } catch (...) {
    return EXIT_FAILURE;
  }
}