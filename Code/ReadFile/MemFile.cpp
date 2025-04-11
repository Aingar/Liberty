#ifdef MEM_FILE_FILESYSTEM

#include "MemFile.h"
#include "DOSFileSystem.h"
#include "TComponent.h"

#include <Windows.h>
#include <stdlib.h>

#define WRITE_BUFFER_SIZE 0x0FFFF

extern ICOManager* DACOM;

extern "C"
{
	IComponentFactory* CreateMemFileFactory(void)
	{
		return new DAComponentFactory<DAComponent<MemoryFile>, MEMFILEDESC>("IFileSystem");
	}

	void Register_MemFile()
	{
		if (IComponentFactory* lpSystem = CreateMemFileFactory())
		{
			DACOM->RegisterComponent(lpSystem, FILESYSTEM_IMPLEMENTATION_NAME);
			lpSystem->Release();
		}
	}
}

MemoryFile::~MemoryFile(void)
{
	if (bOwnedMemory)
	{
		if (bVirtualAlloc)
			VirtualFree(buffer, 0, MEM_RELEASE);
		else
			free(buffer);
	}
	buffer = 0;
}

GENRESULT MemoryFile::CreateInstance(DACOMDESC* descriptor, void** instance)
{
	*instance = 0;
	return GR_GENERIC;
}

BOOL MemoryFile::CloseHandle(HANDLE handle)
{
	return 0;
}

BOOL MemoryFile::ReadFile(HANDLE hFileHandle, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
	BOOL result = 0;

	if (hFileHandle == 0)
	{
		if (lpOverlapped)
			filePos = lpOverlapped->Offset;
		if (filePos > dwEndOfFile)
			filePos = dwEndOfFile;

		U32 maxBytes = dwEndOfFile - filePos;

		if (nNumberOfBytesToRead > maxBytes)
			nNumberOfBytesToRead = maxBytes;

		memcpy(lpBuffer, buffer + filePos, nNumberOfBytesToRead);
		*lpNumberOfBytesRead = nNumberOfBytesToRead;

		filePos += nNumberOfBytesToRead;
		result = 1;
	}

	return result;
}

BOOL MemoryFile::WriteFile(HANDLE hFileHandle, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
	BOOL result = 0;

	if (hFileHandle == 0 && (dwMode & GENERIC_WRITE))
	{
		if (lpOverlapped)
			filePos = lpOverlapped->Offset;

		U32 newFilePos = filePos + nNumberOfBytesToWrite;
		if (newFilePos > bufferSize)
		{
			if (bNoExtend) // limit the write to within the bounds of good taste
			{
				if (filePos >= bufferSize)
					nNumberOfBytesToWrite = 0;
				else
					nNumberOfBytesToWrite = bufferSize - filePos;
			}
			else // expand the write buffer to fit request
			{
				newFilePos = (newFilePos + WRITE_BUFFER_SIZE) & ~0xFFF;

				void* newBuffer;

				if ((newBuffer = VirtualAlloc(0, newFilePos, MEM_COMMIT, PAGE_READWRITE)) == 0)
					nNumberOfBytesToWrite = 0;
				else
				{
					if (buffer)
					{
						// bVirtualAlloc must be TRUE here!
						memcpy(newBuffer, buffer, bufferSize);
						VirtualFree(buffer, 0, MEM_RELEASE);
					}
					bVirtualAlloc = 1;
					buffer = (U8*)newBuffer;
					bufferSize = newFilePos;
				}
			}
		}

		// buffer must now be large enough to handle the write request

		memcpy(buffer + filePos, lpBuffer, nNumberOfBytesToWrite);
		if (lpNumberOfBytesWritten)
			*lpNumberOfBytesWritten = nNumberOfBytesToWrite;

		filePos += nNumberOfBytesToWrite;
		if (nNumberOfBytesToWrite)
			dwEndOfFile = __max(dwEndOfFile, filePos);

		result = 1;
	}

	return result;
}

BOOL MemoryFile::GetOverlappedResult(HANDLE hFileHandle,
	LPOVERLAPPED lpOverlapped,
	LPDWORD lpNumberOfBytesTransferred,
	BOOL bWait)
{
	return 0;
}

DWORD MemoryFile::SetFilePointer(HANDLE hFileHandle, LONG lDistanceToMove,
	PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
	DWORD result = 0xFFFFFFFF;

	if (hFileHandle == 0)
	{
		switch (dwMoveMethod)
		{
		case FILE_BEGIN:
		default:
			break;

		case FILE_CURRENT:
			lDistanceToMove += filePos;
			break;

		case FILE_END:
			lDistanceToMove = bufferSize - lDistanceToMove;
			break;
		}

		// if ((DWORD)lDistanceToMove > bufferSize)
		// lDistanceToMove = bufferSize;

		result = filePos = lDistanceToMove;
		if (lpDistanceToMoveHigh)
			*lpDistanceToMoveHigh = 0;
	}

	return result;
}

BOOL MemoryFile::SetEndOfFile(HANDLE hFileHandle)
{
	BOOL result = 0;

	if (dwMode & GENERIC_WRITE)
	{
		U32 newFilePos = filePos;
		if (newFilePos > bufferSize)
		{
			if (bNoExtend)
			{
				goto Done;
			}
			else // expand the write buffer to fit request
			{
				newFilePos = (newFilePos + WRITE_BUFFER_SIZE) & ~0xFFF;

				void* newBuffer;

				if ((newBuffer = VirtualAlloc(0, newFilePos, MEM_COMMIT, PAGE_READWRITE)) != 0)
				{
					if (buffer)
					{
						// bVirtualAlloc must be TRUE here!
						memcpy(newBuffer, buffer, bufferSize);
						VirtualFree(buffer, 0, MEM_RELEASE);
					}
					bVirtualAlloc = 1;
					buffer = (U8*)newBuffer;
					bufferSize = newFilePos;
				}
			}
		}

		dwEndOfFile = filePos;
		result = 1;
	}
Done:
	return result;
}

DWORD MemoryFile::GetFileSize(HANDLE hFileHandle, LPDWORD lpFileSizeHigh)
{
	DWORD result = 0xFFFFFFFF;

	if (hFileHandle == 0)
	{
		result = dwEndOfFile;
		if (lpFileSizeHigh)
			*lpFileSizeHigh = 0;
	}

	return result;
}

BOOL MemoryFile::LockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh)
{
	return 1;
}

BOOL MemoryFile::UnlockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh, DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh)
{
	return 1;
}

BOOL MemoryFile::GetFileTime(HANDLE hFileHandle, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime, LPFILETIME lpLastWriteTime)
{
	return 0;
}

BOOL MemoryFile::SetFileTime(HANDLE hFileHandle, CONST FILETIME* lpCreationTime, CONST FILETIME* lpLastAccessTime, CONST FILETIME* lpLastWriteTime)
{
	return 0;
}

HANDLE MemoryFile::CreateFileMapping(HANDLE hFileHandle, LPSECURITY_ATTRIBUTES lpFileMappingAttributes, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCTSTR lpName)
{
	return NULL;
}

LPVOID MemoryFile::MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow, DWORD dwNumberOfBytesToMap)
{
	return 0;
}

BOOL MemoryFile::UnmapViewOfFile(LPCVOID lpBaseAddress)
{
	return 0;
}

HANDLE MemoryFile::FindFirstFile(LPCTSTR lpFileName, LPWIN32_FIND_DATA lpFindFileData)
{
	return INVALID_HANDLE_VALUE;
}

BOOL MemoryFile::FindNextFile(HANDLE hFindFile, LPWIN32_FIND_DATA lpFindFileData)
{
	return 0;
}

BOOL MemoryFile::FindClose(HANDLE hFindFile)
{
	return 0;
}

BOOL MemoryFile::CreateDirectory(LPCTSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	return 0;
}

BOOL MemoryFile::RemoveDirectory(LPCTSTR lpPathName)
{
	return 0;
}

DWORD MemoryFile::GetCurrentDirectory(DWORD nBufferLength, LPTSTR lpBuffer)
{
	return 0;
}

BOOL MemoryFile::SetCurrentDirectory(LPCTSTR lpPathName)
{
	return 0;
}

BOOL MemoryFile::DeleteFile(LPCTSTR lpFileName)
{
	return 0;
}

BOOL MemoryFile::CopyFile(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName, BOOL bFailIfExists)
{
	return 0;
}

BOOL MemoryFile::MoveFile(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName)
{
	return 0;
}

DWORD MemoryFile::GetFileAttributes(LPCTSTR lpFileName)
{
	return 0;
}

BOOL MemoryFile::SetFileAttributes(LPCTSTR lpFileName, DWORD dwFileAttributes)
{
	return 0;
}

DWORD MemoryFile::GetLastError(VOID)
{
	return NO_ERROR;
}

HANDLE MemoryFile::OpenChild(DAFILEDESC* lpDesc)
{
	return INVALID_HANDLE_VALUE;
}

DWORD MemoryFile::GetFilePosition(HANDLE hFileHandle, PLONG pPositionHigh)
{
	DWORD result = 0xFFFFFFFF;

	if (hFileHandle == 0)
	{
		if (pPositionHigh)
			*pPositionHigh = 0;
		result = filePos;
	}

	return result;
}

LONG MemoryFile::GetFileName(LPSTR lpBuffer, LONG lBufferSize)
{
	LONG len = (LONG)strlen(fileName) + 1;
	lBufferSize = __min(lBufferSize, len);

	if (lBufferSize > 0 && lpBuffer)
	{
		memcpy(lpBuffer, fileName, lBufferSize);
		lpBuffer[lBufferSize - 1] = 0;
	}

	return lBufferSize;
}

DWORD MemoryFile::GetAccessType(VOID)
{
	return dwMode;
}

GENRESULT MemoryFile::GetParentSystem(LPFILESYSTEM* lplpFileSystem)
{
	*lplpFileSystem = 0;
	return GR_GENERIC;
}

GENRESULT MemoryFile::SetPreference(DWORD dwNumber, DWORD dwValue)
{
	return GR_GENERIC;
}

GENRESULT MemoryFile::GetPreference(DWORD dwNumber, PDWORD pdwValue)
{
	return GR_GENERIC;
}

GENRESULT MemoryFile::ReadDirectoryExtension(HANDLE hFile, LPVOID lpBuffer,
	DWORD nNumberOfBytesToRead,
	LPDWORD lpNumberOfBytesRead, DWORD dwStartOffset)
{
	return GR_GENERIC;
}

GENRESULT MemoryFile::WriteDirectoryExtension(HANDLE hFile, LPCVOID lpBuffer,
	DWORD nNumberOfBytesToWrite,
	LPDWORD lpNumberOfBytesWritten, DWORD dwStartOffset)
{
	return GR_GENERIC;
}

LONG MemoryFile::SerialCall(LPFILESYSTEM lpSystem, DAFILE_SERIAL_PROC lpProc, VOID* lpContext)
{
	return DOS__SerialCall(lpSystem, lpProc, lpContext);
}

void __fastcall switchchar_convert(char* string);

BOOL MemoryFile::GetAbsolutePath(char* lpOutput, LPCTSTR lpInput, LONG lSize)
{
	int len;
	char* ptr;
	BOOL result = 1;

	if (lSize <= 0)
		return 0;

	if (lpInput[0] == '\\')
	{
		strncpy(lpOutput, lpInput, lSize);
		lpOutput[lSize - 1] = 0;
		switchchar_convert(lpOutput);
		return 1;
	}

	// if (iRootIndex)
	strncpy(lpOutput, fileName, lSize);
	// else
	// *lpOutput = 0;

	// now of the form "\\Path\\"

	if (lpInput[0] == '.' && (lpInput[1] == '\\' || lpInput[1] == '/'))
		lpInput += 2;

	while (lpInput[0] == '.' && lpInput[1] == '.')
	{
		len = strlen(lpOutput);
		if (len > 2)
		{
			lpOutput[len - 1] = 0; // get rid of trailing '\\'

			if ((ptr = strrchr(lpOutput, '\\')) != 0)
				ptr[1] = 0;
			else
				result = 0;
		}
		else
			result = 0;
		lpInput += 2;
		if (lpInput[0] == '\\' || lpInput[0] == '/')
			lpInput++;
	}

	len = strlen(lpOutput);
	if (lSize - len > 0)
		strncpy(lpOutput + len, lpInput, lSize - len);
	switchchar_convert(lpOutput);

	return result;
}

GENRESULT MemoryFile::init(MEMFILEDESC* lpDesc)
{
	GENRESULT result = GR_INVALID_PARMS;

	if (lpDesc->lpParent)
		goto Done;

	if (lpDesc->lpImplementation == 0 || strcmp(lpDesc->lpImplementation, "MEM"))
	{
		result = GR_GENERIC;
		goto Done;
	}

	dwMode = lpDesc->dwDesiredAccess;

	if (lpDesc->dwFlags & CMF_DONT_COPY_MEMORY)
	{
		if ((buffer = (U8*)lpDesc->lpBuffer) == 0)
			bufferSize = 0;
		else
			bufferSize = lpDesc->dwBufferSize;
		if (lpDesc->dwFlags & CMF_OWN_MEMORY)
			bOwnedMemory = 1;
		bNoExtend = 1;
	}
	else
	{
		if (lpDesc->lpBuffer)
			bufferSize = lpDesc->dwBufferSize;
		if (bufferSize)
			if ((buffer = (U8*)VirtualAlloc(0, bufferSize, MEM_COMMIT, PAGE_READWRITE)) == 0)
				bufferSize = 0;
		bOwnedMemory = bVirtualAlloc = 1;
	}

	if (buffer)
	{
		switch (lpDesc->dwCreationDistribution)
		{
		case CREATE_NEW:
		case CREATE_ALWAYS:
		case TRUNCATE_EXISTING:
			break; // dwEndOfFile = 0;

		case OPEN_EXISTING:
		case OPEN_ALWAYS:
			dwEndOfFile = bufferSize;
			break;
		}

		if (lpDesc->lpBuffer && lpDesc->lpBuffer != buffer)
			memcpy(buffer, lpDesc->lpBuffer, bufferSize);
	}

	if (lpDesc->lpFileName)
		strncpy(fileName, lpDesc->lpFileName, sizeof(fileName) - 1);

	result = GR_OK;

Done:
	return result;
}

#endif // MEM_FILE_FILESYSTEM
