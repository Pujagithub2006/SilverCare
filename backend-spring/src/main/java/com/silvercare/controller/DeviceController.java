package com.silvercare.controller;

import com.silvercare.dto.ApiResponse;
import com.silvercare.entity.Device;
import com.silvercare.service.DeviceService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;
import java.util.Map;

@RestController
@RequestMapping("/api/devices")
public class DeviceController {

    @Autowired
    private DeviceService deviceService;

    @PostMapping("/register")
    public ResponseEntity<ApiResponse<Device>> registerDevice(@RequestBody Device device) {
        try {
            Device registeredDevice = deviceService.registerDevice(device);
            return ResponseEntity.ok(ApiResponse.<Device>builder()
                    .status("success")
                    .message("Device registered successfully")
                    .data(registeredDevice)
                    .build());
        } catch (Exception e) {
            return ResponseEntity.badRequest()
                    .body(ApiResponse.<Device>error(e.getMessage()));
        }
    }

    @GetMapping("/{deviceId}")
    public ResponseEntity<ApiResponse<Device>> getDevice(@PathVariable String deviceId) {
        try {
            return deviceService.getDevice(deviceId)
                    .map(device -> ResponseEntity.ok(ApiResponse.<Device>builder()
                            .status("success")
                            .data(device)
                            .build()))
                    .orElse(ResponseEntity.notFound().build());
        } catch (Exception e) {
            return ResponseEntity.badRequest()
                    .body(ApiResponse.<Device>error(e.getMessage()));
        }
    }

    @GetMapping
    public ResponseEntity<ApiResponse<List<Device>>> getAllDevices() {
        try {
            List<Device> devices = deviceService.getAllDevices();
            return ResponseEntity.ok(ApiResponse.<List<Device>>builder()
                    .status("success")
                    .data(devices)
                    .build());
        } catch (Exception e) {
            return ResponseEntity.badRequest()
                    .body(ApiResponse.<List<Device>>error(e.getMessage()));
        }
    }

    @GetMapping("/unassigned")
    public ResponseEntity<ApiResponse<List<Device>>> getUnassignedDevices() {
        try {
            List<Device> devices = deviceService.getUnassignedDevices();
            return ResponseEntity.ok(ApiResponse.<List<Device>>builder()
                    .status("success")
                    .data(devices)
                    .build());
        } catch (Exception e) {
            return ResponseEntity.badRequest()
                    .body(ApiResponse.<List<Device>>error(e.getMessage()));
        }
    }

    @GetMapping("/potentially-broken")
    public ResponseEntity<ApiResponse<List<Device>>> getPotentiallyBrokenDevices() {
        try {
            List<Device> devices = deviceService.getPotentiallyBrokenDevices();
            return ResponseEntity.ok(ApiResponse.<List<Device>>builder()
                    .status("success")
                    .data(devices)
                    .message("Devices that haven't sent heartbeat recently")
                    .build());
        } catch (Exception e) {
            return ResponseEntity.badRequest()
                    .body(ApiResponse.<List<Device>>error(e.getMessage()));
        }
    }

    @GetMapping("/mac/{macAddress}")
    public ResponseEntity<ApiResponse<Device>> getDeviceByMac(@PathVariable String macAddress) {
        try {
            return deviceService.getDeviceByMac(macAddress)
                    .map(device -> ResponseEntity.ok(ApiResponse.<Device>builder()
                            .status("success")
                            .data(device)
                            .build()))
                    .orElse(ResponseEntity.notFound().build());
        } catch (Exception e) {
            return ResponseEntity.badRequest()
                    .body(ApiResponse.<Device>error(e.getMessage()));
        }
    }

    @GetMapping("/elderly/{elderlyId}")
    public ResponseEntity<ApiResponse<Map<String, List<Device>>>> getDevicesByElderly(@PathVariable String elderlyId) {
        try {
            Map<String, List<Device>> devices = deviceService.getElderlyDevicesStructured(elderlyId);
            return ResponseEntity.ok(ApiResponse.<Map<String, List<Device>>>builder()
                    .status("success")
                    .data(devices)
                    .build());
        } catch (Exception e) {
            return ResponseEntity.badRequest()
                    .body(ApiResponse.<Map<String, List<Device>>>error(e.getMessage()));
        }
    }

    @PostMapping("/pair")
    public ResponseEntity<ApiResponse<Device>> pairDevice(@RequestBody Map<String, String> request) {
        try {
            String macAddress = request.get("macAddress");
            if (macAddress == null) macAddress = request.get("deviceId");
            String elderlyId = request.get("elderlyId");
            String deviceType = request.get("deviceType");

            Device device = deviceService.pairDeviceToElderly(macAddress, elderlyId, deviceType);
            return ResponseEntity.ok(ApiResponse.<Device>builder()
                    .status("success")
                    .message("Device paired successfully")
                    .data(device)
                    .build());
        } catch (Exception e) {
            return ResponseEntity.badRequest()
                    .body(ApiResponse.<Device>error(e.getMessage()));
        }
    }

    @PostMapping("/unpair")
    public ResponseEntity<ApiResponse<Device>> unpairDevice(@RequestBody Map<String, String> request) {
        try {
            String deviceId = request.get("deviceId");
            if (deviceId == null) deviceId = request.get("macAddress");
            String elderlyId = request.get("elderlyId");
            String reason = request.get("reason");

            Device device = deviceService.unpairDeviceWithReason(deviceId, elderlyId, reason);
            return ResponseEntity.ok(ApiResponse.<Device>builder()
                    .status("success")
                    .message("Device unpaired successfully")
                    .data(device)
                    .build());
        } catch (Exception e) {
            return ResponseEntity.badRequest()
                    .body(ApiResponse.<Device>error(e.getMessage()));
        }
    }

    @PostMapping("/{deviceId}/assign")
    public ResponseEntity<ApiResponse<Device>> assignDevice(
            @PathVariable String deviceId,
            @RequestBody Map<String, String> request) {
        try {
            String elderlyId = request.get("elderlyId");
            Device device = deviceService.assignDeviceToElderly(deviceId, elderlyId);
            return ResponseEntity.ok(ApiResponse.<Device>builder()
                    .status("success")
                    .message("Device assigned successfully")
                    .data(device)
                    .build());
        } catch (Exception e) {
            return ResponseEntity.badRequest()
                    .body(ApiResponse.<Device>error(e.getMessage()));
        }
    }

    @PostMapping("/{deviceId}/unassign")
    public ResponseEntity<ApiResponse<Device>> unassignDevice(@PathVariable String deviceId) {
        try {
            Device device = deviceService.unassignDevice(deviceId);
            return ResponseEntity.ok(ApiResponse.<Device>builder()
                    .status("success")
                    .message("Device unassigned successfully")
                    .data(device)
                    .build());
        } catch (Exception e) {
            return ResponseEntity.badRequest()
                    .body(ApiResponse.<Device>error(e.getMessage()));
        }
    }

    @PostMapping("/{deviceId}/mark-broken")
    public ResponseEntity<ApiResponse<Device>> markDeviceBroken(
            @PathVariable String deviceId,
            @RequestBody Map<String, String> request) {
        try {
            String notes = request.get("notes");
            Device device = deviceService.markDeviceBroken(deviceId, notes);
            return ResponseEntity.ok(ApiResponse.<Device>builder()
                    .status("success")
                    .message("Device marked as broken")
                    .data(device)
                    .build());
        } catch (Exception e) {
            return ResponseEntity.badRequest()
                    .body(ApiResponse.<Device>error(e.getMessage()));
        }
    }

    @PostMapping("/replace")
    public ResponseEntity<ApiResponse<Device>> replaceDevice(@RequestBody Map<String, String> request) {
        try {
            String oldDeviceId = request.get("oldDeviceId");
            String newDeviceId = request.get("newDeviceId");
            Device device = deviceService.replaceDevice(oldDeviceId, newDeviceId);
            return ResponseEntity.ok(ApiResponse.<Device>builder()
                    .status("success")
                    .message("Device replaced successfully")
                    .data(device)
                    .build());
        } catch (Exception e) {
            return ResponseEntity.badRequest()
                    .body(ApiResponse.<Device>error(e.getMessage()));
        }
    }

    @PostMapping("/{deviceId}/heartbeat")
    public ResponseEntity<ApiResponse<Device>> updateHeartbeat(@PathVariable String deviceId) {
        try {
            Device device = deviceService.updateLastSeen(deviceId);
            return ResponseEntity.ok(ApiResponse.<Device>builder()
                    .status("success")
                    .message("Heartbeat updated")
                    .data(device)
                    .build());
        } catch (Exception e) {
            return ResponseEntity.badRequest()
                    .body(ApiResponse.<Device>error(e.getMessage()));
        }
    }

    @PutMapping("/{deviceId}")
    public ResponseEntity<ApiResponse<Device>> updateDevice(
            @PathVariable String deviceId,
            @RequestBody Device deviceUpdates) {
        try {
            Device device = deviceService.updateDevice(deviceId, deviceUpdates);
            return ResponseEntity.ok(ApiResponse.<Device>builder()
                    .status("success")
                    .message("Device updated successfully")
                    .data(device)
                    .build());
        } catch (Exception e) {
            return ResponseEntity.badRequest()
                    .body(ApiResponse.<Device>error(e.getMessage()));
        }
    }

    @DeleteMapping("/{deviceId}")
    public ResponseEntity<ApiResponse<Void>> deleteDevice(@PathVariable String deviceId) {
        try {
            deviceService.deleteDevice(deviceId);
            return ResponseEntity.ok(ApiResponse.<Void>builder()
                    .status("success")
                    .message("Device deleted successfully")
                    .build());
        } catch (Exception e) {
            return ResponseEntity.badRequest()
                    .body(ApiResponse.<Void>error(e.getMessage()));
        }
    }
}
