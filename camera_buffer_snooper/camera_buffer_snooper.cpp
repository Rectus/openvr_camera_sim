
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
//#include <signal.h>


#include "vr_blockqueue_client.h"

int main()
{
	std::cout << "OpenVR camera block queue snooper\n\n";

	vr::EVRInitError initError;
	vr::IVRSystem* vrSystem = vr::VR_Init(&initError, vr::VRApplication_Background);

	if (initError != vr::VRInitError_None)
	{
		std::cerr << "VR_Init failed " << vr::VR_GetVRInitErrorAsSymbol(initError) << std::endl;
		return 1;
	}
	std::cout << "VR_Init succeded\n";

	bool bRun = true;


	vr::PropertyContainerHandle_t rawFrameQueue = 0;

	vr::EBlockQueueError queueError = vr::VRBlockQueue()->Connect(&rawFrameQueue, "/lighthouse/camera/raw_frames");
	if (queueError != vr::EBlockQueueError_BlockQueueError_None)
	{
		std::cerr << "Error connecting to block queue:  " << (int)queueError << std::endl;
		bRun = false;
		rawFrameQueue = 0;
	}

	std::cout << "Connected to block queue /lighthouse/camera/raw_frames " << (uint64_t)rawFrameQueue << std::endl;


	vr::PathHandle_t formatHandle;
	vr::VRPaths()->StringToHandle(&formatHandle, "/format");

	vr::PathHandle_t widthHandle;
	vr::VRPaths()->StringToHandle(&widthHandle, "/width");

	vr::PathHandle_t heightHandle;
	vr::VRPaths()->StringToHandle(&heightHandle, "/height");

	vr::PathHandle_t frameSizeHandle;
	vr::VRPaths()->StringToHandle(&frameSizeHandle, "/frame_size");

	vr::PathHandle_t frameSequenceHandle;
	vr::VRPaths()->StringToHandle(&frameSequenceHandle, "/frame_sequence");

	vr::PathHandle_t frameTimeMonotonicHandle;
	vr::VRPaths()->StringToHandle(&frameTimeMonotonicHandle, "/frame_time_monotonic");

	vr::PathHandle_t serverTimeTicksHandle;
	vr::VRPaths()->StringToHandle(&serverTimeTicksHandle, "/server_time_ticks");

	vr::PathHandle_t deliveryRateHandle;
	vr::VRPaths()->StringToHandle(&deliveryRateHandle, "/delivery_rate");

	vr::PathHandle_t elapsedTimeHandle;
	vr::VRPaths()->StringToHandle(&elapsedTimeHandle, "/elapsed_time");

	std::cout << std::endl << "Static paths:" << std::endl;

	{
		int32_t format = 0;
		int32_t height = 0;
		int32_t width = 0;

		vr::ETrackedPropertyError propError;

		vr::PathRead_t read = {};
		read.unRequiredBufferSize = 0;
		read.pszPath = nullptr;


		read.ulPath = formatHandle;
		read.pvBuffer = &format;
		read.unBufferSize = sizeof(format);
		read.unTag = vr::k_unInt32PropertyTag;

		propError = vr::VRPaths()->ReadPathBatch(rawFrameQueue, &read, 1);
		if (propError != vr::TrackedProp_Success)
		{
			std::cerr << "Error reading /format: " << (int)propError << std::endl;
		}
		std::cout << "/format " << *(int32_t*)read.pvBuffer << std::endl;

		read.ulPath = heightHandle;
		read.pvBuffer = &height;
		read.unBufferSize = sizeof(height);
		read.unTag = vr::k_unInt32PropertyTag;

		propError = vr::VRPaths()->ReadPathBatch(rawFrameQueue, &read, 1);
		if (propError != vr::TrackedProp_Success)
		{
			std::cerr << "Error reading /height: " << (int)propError << std::endl;
		}
		std::cout << "/height " << *(int32_t*)read.pvBuffer << std::endl;

		read.ulPath = widthHandle;
		read.pvBuffer = &width;
		read.unBufferSize = sizeof(width);
		read.unTag = vr::k_unInt32PropertyTag;

		propError = vr::VRPaths()->ReadPathBatch(rawFrameQueue, &read, 1);
		if (propError != vr::TrackedProp_Success)
		{
			std::cerr << "Error reading /width: " << (int)propError << std::endl;
		}
		std::cout << "/width " << *(int32_t*)read.pvBuffer << std::endl;
	}

	std::cout << std::endl;

	HANDLE stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
	DWORD numInputEvents;
	DWORD charactersRead;
	INPUT_RECORD inputRecord;

	LARGE_INTEGER startTime;

	QueryPerformanceCounter(&startTime);

	while (bRun)
	{
		// Break on any key press
		GetNumberOfConsoleInputEvents(stdinHandle, &numInputEvents);
		while (numInputEvents-- > 0)
		{
			ReadConsoleInput(stdinHandle, &inputRecord, 1, &charactersRead);
			if (inputRecord.EventType == KEY_EVENT && ((KEY_EVENT_RECORD&)inputRecord.Event).bKeyDown)
			{
				bRun = false;
				break;
			}
		}
		if (!bRun) { break; }
		
		vr::PropertyContainerHandle_t readHandle;
		uint8_t* pBuffer;

		/*bool bHasReaders = false;
		queueError = vr::VRBlockQueue()->QueueHasReader(rawFrameQueue, &bHasReaders);
		if (queueError != vr::EBlockQueueError_BlockQueueError_None)
		{
			std::cerr << "QueueHasReader error: " << (int)queueError << std::endl;
			bRun = false;
		}
		else
		{
			std::cout << "Queue has reader: " << bHasReaders << std::endl;
		}*/

		queueError = vr::VRBlockQueue()->WaitAndAcquireReadOnlyBlock(rawFrameQueue, &readHandle, (void**)&pBuffer, vr::EBlockQueueReadType_BlockQueueRead_Next, 100);
		if (queueError == vr::EBlockQueueError_BlockQueueError_BlockNotAvailable)
		{
			//std::cout << "No block available" << std::endl;
			continue;
		}
		else if (queueError != vr::EBlockQueueError_BlockQueueError_None)
		{
			std::cerr << "AcquireReadOnlyBlock error: " << (int)queueError << std::endl;
			bRun = false;
		}

		std::cout << "Read-only block acquired: 0x" << std::hex <<(uint64_t)pBuffer << std::dec << std::endl;


		LARGE_INTEGER currTime, perfFrequency;

		QueryPerformanceCounter(&currTime);
		QueryPerformanceFrequency(&perfFrequency);

		double appTimeS = (currTime.QuadPart - startTime.QuadPart) / (double)perfFrequency.QuadPart;
		double appTimeMonotonicS = currTime.QuadPart / (double)perfFrequency.QuadPart;
		std::cout << "Time: " << appTimeS << " " << appTimeMonotonicS << std::endl;
		
		int32_t frameSize = 0;
		uint64_t frameSequence = 0;
		double elapsedTime = 0;
		double frameTimeMonotonic = 0;
		uint64_t serverTimeTicks = 0;
		double deliveryRate = 0;

		
		vr::ETrackedPropertyError propError;

		vr::PathRead_t read = {};
		read.unRequiredBufferSize = 0;
		read.pszPath = nullptr;


		/*read.ulPath = formatHandle;
		read.pvBuffer = &format;
		read.unBufferSize = sizeof(format);
		read.unTag = vr::k_unInt32PropertyTag;

		vr::ETrackedPropertyError propError = vr::VRPaths()->ReadPathBatch(readHandle, &read, 1);
		if (propError != vr::TrackedProp_Success)
		{
			std::cerr << "Error reading /format: " << (int)propError << std::endl;
		}
		std::cout << "/format " << *(int32_t*)read.pvBuffer << std::endl;

		read.ulPath = heightHandle;
		read.pvBuffer = &height;
		read.unBufferSize = sizeof(height);
		read.unTag = vr::k_unInt32PropertyTag;

		propError = vr::VRPaths()->ReadPathBatch(readHandle, &read, 1);
		if (propError != vr::TrackedProp_Success)
		{
			std::cerr << "Error reading /height: " << (int)propError << std::endl;
		}
		std::cout << "/height " << *(int32_t*)read.pvBuffer << std::endl;

		read.ulPath = widthHandle;
		read.pvBuffer = &width;
		read.unBufferSize = sizeof(width);
		read.unTag = vr::k_unInt32PropertyTag;

		propError = vr::VRPaths()->ReadPathBatch(readHandle, &read, 1);
		if (propError != vr::TrackedProp_Success)
		{
			std::cerr << "Error reading /width: " << (int)propError << std::endl;
		}
		std::cout << "/width " << *(int32_t*)read.pvBuffer << std::endl;*/

		read.ulPath = frameSizeHandle;
		read.pvBuffer = &frameSize;
		read.unBufferSize = sizeof(frameSize);
		read.unTag = vr::k_unInt32PropertyTag;

		propError = vr::VRPaths()->ReadPathBatch(readHandle, &read, 1);
		if (propError != vr::TrackedProp_Success)
		{
			std::cerr << "Error reading /frame_size: " << (int)propError << std::endl;
		}
		std::cout << "/frame_size " << *(int32_t*)read.pvBuffer << std::endl;

		read.ulPath = frameSequenceHandle;
		read.pvBuffer = &frameSequence;
		read.unBufferSize = sizeof(frameSequence);
		read.unTag = vr::k_unUint64PropertyTag;

		propError = vr::VRPaths()->ReadPathBatch(readHandle, &read, 1);
		if (propError != vr::TrackedProp_Success)
		{
			std::cerr << "Error reading /frame_sequence: " << (int)propError << std::endl;
		}
		std::cout << "/frame_sequence " << *(uint64_t*)read.pvBuffer << std::endl;


		read.ulPath = frameTimeMonotonicHandle;
		read.pvBuffer = &frameTimeMonotonic;
		read.unBufferSize = sizeof(frameTimeMonotonic);
		read.unTag = vr::k_unDoublePropertyTag;

		propError = vr::VRPaths()->ReadPathBatch(readHandle, &read, 1);
		if (propError != vr::TrackedProp_Success)
		{
			std::cerr << "Error reading /frame_time_monotonic: " << (int)propError << std::endl;
		}
		std::cout << "/frame_time_monotonic " << *(double*)read.pvBuffer << std::endl;

		read.ulPath = serverTimeTicksHandle;
		read.pvBuffer = &serverTimeTicks;
		read.unBufferSize = sizeof(serverTimeTicks);
		read.unTag = vr::k_unUint64PropertyTag;

		propError = vr::VRPaths()->ReadPathBatch(readHandle, &read, 1);
		if (propError != vr::TrackedProp_Success)
		{
			std::cerr << "Error reading /server_time_ticks: " << (int)propError << std::endl;
		}
		std::cout << "/server_time_ticks " << *(uint64_t*)read.pvBuffer << std::endl;

		read.ulPath = deliveryRateHandle;
		read.pvBuffer = &deliveryRate;
		read.unBufferSize = sizeof(deliveryRate);
		read.unTag = vr::k_unDoublePropertyTag;

		propError = vr::VRPaths()->ReadPathBatch(readHandle, &read, 1);
		if (propError != vr::TrackedProp_Success)
		{
			std::cerr << "Error reading /delivery_rate: " << (int)propError << std::endl;
		}
		std::cout << "/delivery_rate " << *(double*)read.pvBuffer << std::endl;

		read.ulPath = elapsedTimeHandle;
		read.pvBuffer = &elapsedTime;
		read.unBufferSize = sizeof(elapsedTime);
		read.unTag = vr::k_unDoublePropertyTag;

		propError = vr::VRPaths()->ReadPathBatch(readHandle, &read, 1);
		if (propError != vr::TrackedProp_Success)
		{
			std::cerr << "Error reading /elapsed_time: " << (int)propError << std::endl;
		}
		std::cout << "/elapsed_time " << *(double*)read.pvBuffer << std::endl;


		queueError = vr::VRBlockQueue()->ReleaseReadOnlyBlock(rawFrameQueue, readHandle);
		if (queueError != vr::EBlockQueueError_BlockQueueError_None)
		{
			std::cerr << "ReleaseReadOnlyBlock error: " << (int)queueError << std::endl;
		}

		std::cout << std::endl;
	}

	if (rawFrameQueue != 0)
	{
		queueError = vr::VRBlockQueue()->Destroy(rawFrameQueue);
		if (queueError != vr::EBlockQueueError_BlockQueueError_None)
		{
			std::cerr << "Queue Destroy error: " << (int)queueError << std::endl;
		}
	}

	vr::VR_Shutdown();

	std::cout << "Shutdown complete"  << std::endl;

	return 0;
}

