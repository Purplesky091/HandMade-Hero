/* ========================================================================
$File: $
$Date: $
$Revision: $
$Creator: Casey Muratori $
$Notice: (C) Copyright 2014 by Molly Rocket, Inc. All Rights Reserved. $
======================================================================== */

#include <windows.h>
#include <stdint.h>
#include <Xinput.h>

#define internal static 
#define local_persist static 
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void *BitmapMemory;
	int BitmapWidth;
	int BitmapHeight;
	int Pitch;
};

struct win32_window_dimension
{
	int Width;
	int Height;
};

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex,  XINPUT_STATE* pState) //if you pass the macro a parameter, it's going to define a function with that name
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex,  XINPUT_VIBRATION* pVibration)
typedef X_INPUT_GET_STATE(x_input_get_state);
typedef X_INPUT_SET_STATE(x_input_set_state);

X_INPUT_GET_STATE(XInputGetStateStub)
{
	return 0;
}

X_INPUT_SET_STATE(XInputSetStateStub)
{
	return 0;
}

global_variable x_input_get_state* XInputGetState_; //makes a pointer to a function with the x_input_get_state function signature
global_variable x_input_set_state* XInputSetState_;
#define XInputGetState XInputGetState_;
#define XInputSetState XinputSetState_;

// TODO(casey): This is a global for now.
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer globalBuffer;



win32_window_dimension Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;

	RECT ClientRect; //ClientRect = drawable area of our window. So our window - the border
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return Result;
}

internal void
RenderWeirdGradient(win32_offscreen_buffer Buffer, int BlueOffset, int GreenOffset)
{
	int Width = Buffer.BitmapWidth;
	int Height = Buffer.BitmapHeight;

	//int Pitch = Width*Buffer.BytesPerPixel;
	uint8 *Row = (uint8 *)Buffer.BitmapMemory;
	for (int Y = 0; Y < Height; ++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int X = 0; X < Width; ++X)
		{
			/*
			Since RGB value is a 32 bit value, we just have two 8-bit values be our blue and green value.
			Then, we write our bits and OR them together and then shift whichever bits need to be shifted.
			*/
			uint8 Blue = X + BlueOffset;
			uint8 Green = Y + GreenOffset;

			*Pixel++ = (Blue | (Green << 8));
		}

		Row += Buffer.Pitch;
	}
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
	// TODO(casey): Bulletproof this.
	// Maybe don't free first, free after, then free first if that fails.

	if (Buffer->BitmapMemory)
	{
		VirtualFree(Buffer->BitmapMemory, 0, MEM_RELEASE);
	}

	Buffer->BitmapWidth = Width;
	Buffer->BitmapHeight = Height;
	int BytesPerPixel = 4;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->BitmapWidth;
	Buffer->Info.bmiHeader.biHeight = -Buffer->BitmapHeight;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	// NOTE(casey): Thank you to Chris Hecker of Spy Party fame
	// for clarifying the deal with StretchDIBits and BitBlt!
	// No more DC for us.
	int BitmapMemorySize = (Buffer->BitmapWidth*Buffer->BitmapHeight)*BytesPerPixel;
	Buffer->BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE); // this will reserve the amount of pages needed to hold BitMapMemorySize
	Buffer->Pitch = Width * BytesPerPixel; // pitch = how many bytes a pointer has to move to get from one row (of pixels) to the next row.
	//width = how wide a row is. row width * how many bytes is a pixel = the pitch.
	// TODO(casey): Probably clear this to black
}

internal void
Win32DisplayBufferInWindow(HDC DeviceContext, int WindowWidth, int WindowHeight,
	win32_offscreen_buffer Buffer)
{
	//TODO - aspect ratio correction.
	StretchDIBits(DeviceContext,
		/*
		X, Y, Width, Height,
		X, Y, Width, Height,
		*/
		0, 0, WindowWidth, WindowHeight,
		0, 0, Buffer.BitmapWidth, Buffer.BitmapHeight,
		Buffer.BitmapMemory,
		&Buffer.Info,
		DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam)
{
	LRESULT Result = 0;

	switch (Message)
	{
	case WM_SIZE:
	{
	} break;

	case WM_CLOSE:
	{
		// TODO(casey): Handle this with a message to the user?
		GlobalRunning = false;
	} break;

	case WM_ACTIVATEAPP:
	{
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	} break;

	case WM_DESTROY:
	{
		// TODO(casey): Handle this as an error - recreate window?
		GlobalRunning = false;
	} break;
	/*
	Windows sometimes will block our process to do a resize.
	Whenever windows blocks our process, windows will tell us to repaint.
	THis is the repaint that is called.
	We do the same blit in the main()
	*/
	case WM_PAINT: 
	{
		PAINTSTRUCT Paint;
		HDC DeviceContext = BeginPaint(Window, &Paint);
		int X = Paint.rcPaint.left;
		int Y = Paint.rcPaint.top;
		win32_window_dimension Dimension = Win32GetWindowDimension(Window);

		Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, globalBuffer);
		EndPaint(Window, &Paint);
		//BeginPaint ~ EndPaint tells windows that you have updated the dirty region
	} break;

	default:
	{
		Result = DefWindowProc(Window, Message, WParam, LParam);
	} break;
	}

	return(Result);
}


int CALLBACK
WinMain(HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR     CmdLine,
	int       showCode)
{
	// zero is initialization.
	/*
	Empty brackets means clear the memory this struct occupies to 0
	*/
	WNDCLASS windowClass = {};
	Win32ResizeDIBSection(&globalBuffer, 1280, 720);

	windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	windowClass.lpfnWndProc = Win32MainWindowCallback;
	windowClass.hInstance = Instance;
	windowClass.lpszClassName = L"HandmadeHeroWindowClass";

	if (RegisterClass(&windowClass))
	{
		HWND Window =
			CreateWindowEx(
				0,
				windowClass.lpszClassName,
				L"Handmade Hero",
				WS_OVERLAPPEDWINDOW | WS_VISIBLE,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				0,
				0,
				Instance,
				0);


		if (Window)
		{
			int XOffset = 0;
			int YOffset = 0;
			HDC DeviceContext = GetDC(Window);
			GlobalRunning = true;
			while (GlobalRunning)
			{
				MSG message;
				while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
				{
					if (message.message == WM_QUIT)
					{
						GlobalRunning = false;
					}

					TranslateMessage(&message);
					DispatchMessage(&message);
				}

				//TODO - should we poll this more frequently?
				for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ControllerIndex++)
				{
					XINPUT_STATE ControllerState;
					if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
					{
						//this controller is plugged in
						// see if controllerstate.dwpacketstate increments too rapidly
						XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;
						bool Up = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
						bool Down = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
						bool Left = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
						bool Right = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
						bool Start = Pad->wButtons & XINPUT_GAMEPAD_START;
						bool Back = Pad->wButtons & XINPUT_GAMEPAD_BACK;
						bool LeftShoulder = Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
						bool RightShoulder = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
						bool AButton = Pad->wButtons & XINPUT_GAMEPAD_A;
						bool BButton = Pad->wButtons & XINPUT_GAMEPAD_B;
						bool XButton = Pad->wButtons & XINPUT_GAMEPAD_X;
						bool yButton = Pad->wButtons & XINPUT_GAMEPAD_Y;

						int16 StickX = Pad->sThumbLX;
						int16 StickY = Pad->sThumbLY;
					}
					else
					{
						// controller is not available
					}
				}

				RenderWeirdGradient(globalBuffer, XOffset, YOffset);
				XOffset++;
				// we force blit the buffer to the scren after we process messages

				win32_window_dimension Dimension = Win32GetWindowDimension(Window);
				Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, globalBuffer);
			}
		}
		else
		{
			//TODO: LOGGING
		}
	}
	else
	{

	}

	return 0;
}