#include "stdafx.h"
#include "File.h"
#include "FileOutputStream.h"
#if defined(__ANDROID__)
#include <cstdio>
#endif

//Creates a file output stream to write to the file represented by the specified File object. A new FileDescriptor object is
//created to represent this file connection.
//First, if there is a security manager, its checkWrite method is called with the path represented by the file argument as its argument.
//
//If the file exists but is a directory rather than a regular file, does not exist but cannot be created, or cannot be opened
//for any other reason then a FileNotFoundException is thrown.
//
//Parameters:
//file - the file to be opened for writing.
FileOutputStream::FileOutputStream(const File &file)
#if defined(__ANDROID__)
	: m_fileHandle(NULL)
#else
	: m_fileHandle(INVALID_HANDLE_VALUE)
#endif
{
	if( file.exists() && file.isDirectory())
	{
		// TODO 4J Stu - FileNotFoundException
		return;
	}

#if defined(__ANDROID__)
	m_fileHandle = static_cast<HANDLE>(std::fopen(wstringtofilename(file.getPath()), "wb"));
#elif defined(_DURANGO)
	m_fileHandle = CreateFile(
		file.getPath().c_str() , // file name
		GENERIC_WRITE, // access mode
		0, // share mode // TODO 4J Stu - Will we need to share file? Probably not but...
		NULL, // Unused
		OPEN_ALWAYS , // how to create
		FILE_ATTRIBUTE_NORMAL , // file attributes
		NULL // Unsupported
		);
#else
	m_fileHandle = CreateFile(
		wstringtofilename(file.getPath()) , // file name
		GENERIC_WRITE, // access mode
		0, // share mode // TODO 4J Stu - Will we need to share file? Probably not but...
		NULL, // Unused
		OPEN_ALWAYS , // how to create
		FILE_ATTRIBUTE_NORMAL , // file attributes
		NULL // Unsupported
		);
#endif

	if (
#if defined(__ANDROID__)
		m_fileHandle == NULL
#else
		m_fileHandle == INVALID_HANDLE_VALUE
#endif
	)
	{
#if defined(__ANDROID__)
		app.DebugPrintf("Android FileOutputStream: could not open %s\\n", wstringtofilename(file.getPath()));
#else
		DWORD error = GetLastError();
#endif
		// TODO 4J Stu - Any form of error/exception handling
	}
}

FileOutputStream::~FileOutputStream()
{
#if defined(__ANDROID__)
	if (m_fileHandle != NULL) std::fclose(static_cast<FILE*>(m_fileHandle));
#else
	if( m_fileHandle != INVALID_HANDLE_VALUE )
		CloseHandle( m_fileHandle );
#endif
}

//Writes the specified byte to this file output stream. Implements the write method of OutputStream.
//Parameters:
//b - the byte to be written.
void FileOutputStream::write(unsigned int b)
{
#if defined(__ANDROID__)
	if (m_fileHandle == NULL) return;
	const unsigned char value = static_cast<unsigned char>(b);
	(void)std::fwrite(&value, 1, 1, static_cast<FILE*>(m_fileHandle));
#else
	DWORD numberOfBytesWritten;

	byte value = (byte) b;

	BOOL result = WriteFile(
		m_fileHandle, // handle to file
		&value, // data buffer
		1, // number of bytes to write
		&numberOfBytesWritten, // number of bytes written
		NULL // overlapped buffer
		);

	if( result == 0 )
	{
		// TODO 4J Stu - Some kind of error handling
	}
	else if( numberOfBytesWritten == 0 )
	{
		// File pointer is past the end of the file
	}
#endif
}

//Writes b.length bytes from the specified byte array to this file output stream.
//Parameters:
//b - the data.
void FileOutputStream::write(byteArray b)
{
#if defined(__ANDROID__)
	if (m_fileHandle == NULL || b.data == NULL || b.length == 0) return;
	(void)std::fwrite(b.data, 1, b.length, static_cast<FILE*>(m_fileHandle));
#else
	DWORD numberOfBytesWritten;

	BOOL result = WriteFile(
		m_fileHandle, // handle to file
		&b.data, // data buffer
		b.length, // number of bytes to write
		&numberOfBytesWritten, // number of bytes written
		NULL // overlapped buffer
		);

	if( result == 0 )
	{
		// TODO 4J Stu - Some kind of error handling
	}
	else if( numberOfBytesWritten == 0 || numberOfBytesWritten != b.length  )
	{
		// File pointer is past the end of the file
	}
#endif
}

//Writes len bytes from the specified byte array starting at offset off to this file output stream.
//Parameters:
//b - the data.
//off - the start offset in the data.
//len - the number of bytes to write.
void FileOutputStream::write(byteArray b, unsigned int offset, unsigned int length)
{
#if defined(__ANDROID__)
	if (m_fileHandle == NULL || b.data == NULL || offset > b.length || length == 0) return;
	if (length > b.length - offset) length = b.length - offset;
	(void)std::fwrite(&b[offset], 1, length, static_cast<FILE*>(m_fileHandle));
#else
	assert(length <= (b.length - offset));
	DWORD numberOfBytesWritten;
	BOOL result = WriteFile(m_fileHandle, &b[offset], length, &numberOfBytesWritten, NULL);
	if (result == 0 || numberOfBytesWritten != length) {
		// TODO 4J Stu - report the storage write failure.
	}
#endif
}

// Closes the stream and releases its file handle.
void FileOutputStream::close()
{
#if defined(__ANDROID__)
	if (m_fileHandle == NULL) return;
	std::fflush(static_cast<FILE*>(m_fileHandle));
	std::fclose(static_cast<FILE*>(m_fileHandle));
	m_fileHandle = NULL;
#else
	if (m_fileHandle == INVALID_HANDLE_VALUE) return;
	CloseHandle(m_fileHandle);
	m_fileHandle = INVALID_HANDLE_VALUE;
#endif
}
