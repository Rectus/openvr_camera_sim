OpenVR Camera Simulation Driver
---

This SteamVR driver showcases a basic implementation of a simulated HMD with working passthrough cameras. It creates a simulated HMD using the `IVRDisplayComponent` , `IVRVirtualDisplay`, and `IVRCameraComponent` classes, displaying the headset output in a custom desktop window. 


The passthrough camera systems in SteamVR seem to be hardcoded for the lighthouse driver, and only using the `IVRCameraComponent` is not sufficient for SteamVR to process the passthrough. The camera framebuffers and per-frame data are instead submitted using the `IVRBlockQueue` and `IVRPaths` classes, only publicly available in the OpenVR C (not C++) headers.


The camera output works with both the 2D Room View mode, and applications using the `IVRTrackedCamera` interface. The stereo correct 3D Room View mode is not currently supported.


Note: I have not verified if all the projection parameters are correct with a real camera.


### Main files of interest
- camera_component.cpp - Camera implementation
- camera_device.cpp - HMD implementation
- vr_blockqueue.h - C++ header versions of `IVRBlockQueue` and `IVRPaths`



The repo also contains `camera_buffer_snooper`, a client utility that prints out any frame metadata sent to the block queue.


### Camera Distortion

The Valve Index uses fisheye lenses modeled using equidistant (f-theta) projection, where r = f * theta.

The distortion model used is the Kannala-Brandt model, described in [1], using only the radial distortion component. The coefficients are shifted by one, with the first power coeffcients locked to 1, and with the 3rd to 9th power coeffcients being mapped to the four coeffcients provided. this is the same model used by the [OpenCV fisheye library](https://docs.opencv.org/4.x/db/d58/group__calib3d__fisheye.html). 

The runtime uses the output of `IVRCameraComponent::GetCameraDistortion()` to undistort the Room View and undistorted frame formats of `IVRTrackedCamera`. Since the function can be implemented using arbitrary distortion functions, applications using only the undistorted frames should work with any model. 

However, applications that use distorted frames, such as Valve's [hmd_opencv_sandbox](https://github.com/ValveSoftware/openvr/tree/master/samples/hmd_opencv_sandbox), and [openxr-steamvr-passthrough](https://github.com/Rectus/openxr-steamvr-passthrough), can only receive the distortion parameters using the `Prop_CameraDistortionFunction_Int32_Array` and `Prop_CameraDistortionCoefficients_Float_Array` properties. This limits them to only being able to support the single lens model listed in `EVRDistortionFunctionType`, which is the above one.


### References

[1] Kannala, J., & Brandt, S. S. (2006). A generic camera model and calibration method for conventional, wide-angle, and fish-eye lenses\
https://users.aalto.fi/~kannalj1/publications/tpami2006.pdf
