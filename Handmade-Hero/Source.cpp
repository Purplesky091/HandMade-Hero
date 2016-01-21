/* ========================================================================
$File: $
$Date: $
$Revision: $
$Creator: Casey Muratori $
$Notice: (C) Copyright 2014 by Molly Rocket, Inc. All Rights Reserved. $
======================================================================== */

#include <windows.h>
#include <stdint.h>

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
	int BytesPerPixel;
	int Pitch;
};
// TODO(casey): This is a global for now.
global_variable bool Running;
global_variable win32_offscreen_buffer globalBuffer;

struct win32_window_dimension
{
	int Width;
	int Height;
};

win32_window_dimension Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;

	RECT ClientRect;
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
	for (int Y = 0; Y < Buffer.BitmapHeight; ++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int X = 0; X < Buffer.BitmapWidth; ++X)
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
	Buffer->BytesPerPixel = 4;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->BitmapWidth;
	Buffer->Info.bmiHeader.biHeight = -Buffer->BitmapHeight;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	// NOTE(casey): Thank you to Chris Hecker of Spy Party fame
	// for clarifying the deal with StretchDIBits and BitBlt!
	// No more DC for us.
	int BitmapMemorySize = (Buffer->BitmapWidth*Buffer->BitmapHeight)*Buffer->BytesPerPixel;
	Buffer->BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	Buffer->Pitch = Width * Buffer->BytesPerPixel;
	// TODO(casey): Probably clear this to black
}

internal void
Win32DisplayBufferInWindow(HDC DeviceContext, int WindowWidth, int WindowHeight,
	win32_offscreen_buffer Buffer, int X, int Y)
{
	StretchDIBits(DeviceContext,
		/*
		X, Y, Width, Height,
		X, Y, Width, Height,
		*/
		0, 0, Buffer.BitmapWidth, Buffer.BitmapHeight,
		0, 0, WindowWidth, WindowHeight,
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
		win32_window_dimension Dimension = Win32GetWindowDimension(Window);
		Win32ResizeDIBSection(&globalBuffer, Dimension.Width, Dimension.Height);
	} break;

	case WM_CLOSE:
	{
		// TODO(casey): Handle this with a message to the user?
		Running = false;
	} break;

	case WM_ACTIVATEAPP:
	{
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	} break;

	case WM_DESTROY:
	{
		// TODO(casey): Handle this as an error - recreate window?
		Running = false;
	} break;

	case WM_PAINT:
	{
		PAINTSTRUCT Paint;
		HDC DeviceContext = BeginPaint(Window, &Paint);
		int X = Paint.rcPaint.left;
		int Y = Paint.rcPaint.top;
		win32_window_dimension Dimension = Win32GetWindowDimension(Window);

		Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, globalBuffer, X, Y);
		EndPaint(Window, &Paint);
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
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
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

			Running = true;
			while (Running)
			{
				MSG message;
				while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
				{
					if (message.message == WM_QUIT)
					{
						Running = false;
					}

					TranslateMessage(&message);
					DispatchMessage(&message);
				}

				RenderWeirdGradient(globalBuffer, XOffset, YOffset);
				XOffset++;

				HDC DeviceContext = GetDC(Window);
				win32_window_dimension Dimension = Win32GetWindowDimension(Window);
				Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, globalBuffer, 0, 0);
				ReleaseDC(Window, DeviceContext);
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