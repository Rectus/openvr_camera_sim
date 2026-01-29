#pragma once


class CameraComponent : public vr::IVRCameraComponent
{
public:

	CameraComponent();
	~CameraComponent();

	bool Init(vr::TrackedDeviceIndex_t HMDDeviceId);
	void Deinit();

	std::string& GetCameraDisplayName()
	{
		return m_cameraName;
	}

	// Inherited from IVRCameraComponent
	virtual bool GetCameraFrameDimensions(vr::ECameraVideoStreamFormat nVideoStreamFormat, uint32_t* pWidth, uint32_t* pHeight) override;
	virtual bool GetCameraFrameBufferingRequirements(int* pDefaultFrameQueueSize, uint32_t* pFrameBufferDataSize) override;
	virtual bool SetCameraFrameBuffering(int nFrameBufferCount, void** ppFrameBuffers, uint32_t nFrameBufferDataSize) override;
	virtual bool SetCameraVideoStreamFormat(vr::ECameraVideoStreamFormat nVideoStreamFormat) override;
	virtual vr::ECameraVideoStreamFormat GetCameraVideoStreamFormat() override;
	virtual bool StartVideoStream() override;
	virtual void StopVideoStream() override;
	virtual bool IsVideoStreamActive(bool* pbPaused, float* pflElapsedTime) override;
	virtual const vr::CameraVideoStreamFrame_t* GetVideoStreamFrame() override;
	virtual void ReleaseVideoStreamFrame(const vr::CameraVideoStreamFrame_t* pFrameImage) override;
	virtual bool SetAutoExposure(bool bEnable) override;
	virtual bool PauseVideoStream() override;
	virtual bool ResumeVideoStream() override;
	virtual bool GetCameraDistortion(uint32_t nCameraIndex, float flInputU, float flInputV, float* pflOutputU, float* pflOutputV) override;
	virtual bool GetCameraProjection(uint32_t nCameraIndex, vr::EVRTrackedCameraFrameType eFrameType, float flZNear, float flZFar, vr::HmdMatrix44_t* pProjection) override;
	virtual bool SetFrameRate(int nISPFrameRate, int nSensorFrameRate) override;
	virtual bool SetCameraVideoSinkCallback(vr::ICameraVideoSinkCallback* pCameraVideoSinkCallback) override;
	virtual bool GetCameraCompatibilityMode(vr::ECameraCompatibilityMode* pCameraCompatibilityMode) override;
	virtual bool SetCameraCompatibilityMode(vr::ECameraCompatibilityMode nCameraCompatibilityMode) override;
	virtual bool GetCameraFrameBounds(vr::EVRTrackedCameraFrameType eFrameType, uint32_t* pLeft, uint32_t* pTop, uint32_t* pWidth, uint32_t* pHeight) override;
	virtual bool GetCameraIntrinsics(uint32_t nCameraIndex, vr::EVRTrackedCameraFrameType eFrameType, vr::HmdVector2_t* pFocalLength, vr::HmdVector2_t* pCenter, vr::EVRDistortionFunctionType* peDistortionType, double rCoefficients[vr::k_unMaxDistortionFunctionParameters]) override;

protected:
	void ServeFrames();

	bool m_bIsInitialized = false;
	bool m_bIsStreamActive = false;
	bool m_bIsStreamPaused = false;

	vr::TrackedDeviceIndex_t m_HMDDeviceId = -1;
	int m_deviceNum = 0;
	int m_selectedMediaType = -1;
	vr::ECameraVideoStreamFormat m_streamFormat = vr::CVS_FORMAT_UNKNOWN;
	std::string m_cameraName;

	uint32_t m_textureWidth = 0;
	uint32_t m_textureHeight = 0;
	uint32_t m_textureBPP = 0;

	uint32_t m_frameWidth = 0;
	uint32_t m_frameHeight = 0;

	float m_focalLeftX = 0;
	float m_focalLeftY = 0;
	float m_centerLeftX = 0;
	float m_centerLeftY = 0;

	float m_focalRightX = 0;
	float m_focalRightY = 0;
	float m_centerRightX = 0;
	float m_centerRightY = 0;

	std::vector<int32_t> m_distortionFunction;
	std::vector<double> m_distortionCoeff;

	uint64_t m_frameCount = 0;

	uint64_t m_frameSequence = 0;
	double m_frameTimeMonotonic = 0.0;
	uint64_t m_serverFrameTicks = 0;
	double m_deliveryRate = 0.0;
	double m_elapsedTime = 0.0;

	LARGE_INTEGER m_perfCounterFrequency;

	LARGE_INTEGER m_startTime;
	LARGE_INTEGER m_lastFrameTime;
	LARGE_INTEGER m_firstStartTime;
	bool m_bIsFirstStart = true;

	std::thread m_frameServeThread;
	std::atomic<bool> m_bRunThread = true;

	vr::ICameraVideoSinkCallback* m_pCameraVideoSinkCallback = nullptr;

	vr::PropertyContainerHandle_t m_rawFrameQueue = 0;

	vr::PathHandle_t m_frameSequenceHandle;
	vr::PathHandle_t m_frameSizeHandle;
	vr::PathHandle_t m_frameTimeMonotonicHandle;
	vr::PathHandle_t m_serverTimeTicksHandle;
	vr::PathHandle_t m_deliveryRateHandle;
	vr::PathHandle_t m_elapsedTimeHandle;
};