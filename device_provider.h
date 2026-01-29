#pragma once
#include <memory>

#include "camera_device.h"

class DeviceProvider : public vr::IServerTrackedDeviceProvider 
{
public:
    // Inherited via IServerTrackedDeviceProvider
    vr::EVRInitError Init(vr::IVRDriverContext* pDriverContext) override;
    void Cleanup() override;
    const char* const* GetInterfaceVersions() override;
    void RunFrame() override;
    bool ShouldBlockStandbyMode() override;
    void EnterStandby() override;
    void LeaveStandby() override;

private:
    std::unique_ptr<CameraDevice> m_cameraDevice;

};