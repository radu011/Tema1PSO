#undef UNICODE

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "so_stdio.h"

#define BUFFER_SIZE 4096
#define READ_FLAG 1
#define WRITE_FLAG 2

struct _so_file
{
    HANDLE _fileDescriptor;       // FIle descriptor number
    char _openMode[3];         // Opening mode
    int _openingFlags;         // Opening flags
    unsigned char _buffer[BUFFER_SIZE]; // Buffer for I/O
    int _bufferLen;            // Length of buffer
    int _posInBuffer;          // Cursor position in buffer
    long _posInFile;           // Cursor position in file (buffer)
    size_t _realPosInFile;       // Cursor position in file (real)
    BOOL _endOfFile;            // End of file flag, set when cursor reached end of file
    BOOL _errorFlag;            // Error flag, set when error appear -> non-zero value for error
    int _lastOp;               // Last operation executed on file
    int pid;
};

// Open file function
SO_FILE *so_fopen(const char *pathname, const char *mode)
{
    if(pathname == NULL || mode == NULL)
        return NULL;

    int oflag = 0;
    int cflag = 0;

    if (strcmp(mode, "r") == 0)
    {
        //oflag = O_RDONLY;
        oflag = GENERIC_READ;
        cflag = OPEN_EXISTING;
    }
    else if (strcmp(mode, "r+") == 0)
    {
        //oflag = O_RDWR;
        oflag = GENERIC_READ | GENERIC_WRITE;
        cflag = OPEN_EXISTING;
    }
    else if (strcmp(mode, "w") == 0)
    {
        //oflag = O_CREAT | O_TRUNC | O_WRONLY;
        oflag = GENERIC_READ;
        cflag = CREATE_ALWAYS;
    }
    else if (strcmp(mode, "w+") == 0)
    {
        //oflag = O_TRUNC | O_CREAT | O_RDWR;
        oflag = GENERIC_READ | GENERIC_WRITE;
        cflag = CREATE_ALWAYS;
    }
    else if (strcmp(mode, "a") == 0)
    {
        //oflag = O_APPEND | O_CREAT | O_WRONLY;
        oflag = GENERIC_READ;
        cflag = OPEN_ALWAYS;
    }
    else if (strcmp(mode, "a+") == 0)
    {
        //oflag = O_APPEND | O_CREAT | O_RDWR;
        oflag = GENERIC_READ | GENERIC_WRITE;
        cflag = OPEN_ALWAYS;
    }
    else
    {
        return NULL;
    }

    HANDLE fileDesc = CreateFileA(
        pathname,
        oflag,
        NULL,
        NULL,
        cflag,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (fileDesc == INVALID_HANDLE_VALUE)
        return NULL;


    SO_FILE *file = (SO_FILE *)malloc(sizeof(SO_FILE));
    if (file == NULL)
    {
        return NULL;
    }

    file->_fileDescriptor = fileDesc;
    strcpy(file->_openMode, mode);
    file->_posInFile = 0;
    file->_posInBuffer = 0;
    file->_errorFlag = 0;
    strcpy(file->_buffer, "");
    file->_bufferLen = 0;
    file->_endOfFile = 0;
    file->_lastOp = SO_EOF;

    return file;
}

int so_fclose(SO_FILE *stream)
{
    if(!stream)
        return SO_EOF;

    if(stream->_lastOp == WRITE_FLAG)
    {
        int err = so_fflush(stream);
        if(!err)
        {
            stream->_errorFlag = TRUE;
            return stream->_errorFlag;
        }
    }
    
    stream->_errorFlag = CloseHandle(stream->_fileDescriptor);
    if (!stream->_errorFlag)
    {
        free(stream);
        stream = NULL;    
        return stream->_errorFlag;
    }

    free(stream);
    stream = NULL;
    return 0;
}

HANDLE so_fileno(SO_FILE *stream)
{
    return stream->_fileDescriptor;
}

int so_fflush(SO_FILE *stream)
{
    if(stream == NULL)
        return SO_EOF;

    if (stream->_lastOp == WRITE_FLAG)
    {
        int bytesToWrite = stream->_bufferLen;
        int count = 0;
        int bytesWritten = 0;

        while (count < bytesToWrite)
        {
            DWORD bytesWritten;
            int errWrite = WriteFile(
                stream->_fileDescriptor,
                stream->_buffer + count,
                stream->_posInBuffer - count,
                &bytesWritten,
                NULL
            );
            if(!errWrite)
            {
                stream->_errorFlag = TRUE;
                return SO_EOF;
            }

            count += bytesWritten;
        }

        strcpy(stream->_buffer, "");
        stream->_bufferLen = 0;
        stream->_posInBuffer = 0;
        stream->_realPosInFile += count;

        return 0;
    }
    else
    {
        strcpy(stream->_buffer, "");
        stream->_bufferLen = 0;
        stream->_posInBuffer = 0;
        stream->_errorFlag = 0;
        return 0;
    }
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
    if(stream == NULL)
        return SO_EOF;

    if(stream->_lastOp == WRITE_FLAG)
    {
        int err = so_fflush(stream);
        if(-1 == err)
        {
            stream->_errorFlag = TRUE;
            return SO_EOF;
        }
    }
    else
    {
        strcpy(stream->_buffer, "");
        stream->_posInBuffer = 0;
        stream->_bufferLen = 0;
    }

    size_t offsetLocation = SetFilePointer(
        stream->_fileDescriptor,
        offset,
        NULL,
        whence
    );

    if(offsetLocation == INVALID_SET_FILE_POINTER)
    {
        stream->_errorFlag = TRUE;
        return SO_EOF;
    }   

    stream->_lastOp = 0;
    stream->_realPosInFile = offsetLocation;

    return 0;
}

long so_ftell(SO_FILE *stream)
{
    if(stream == NULL)
        return SO_EOF;

    if(stream->_lastOp == WRITE_FLAG)
    {
        int err = so_fflush(stream);
        if(-1 == err)
        {
            stream->_errorFlag = TRUE;
            return SO_EOF;
        }
    }

    return stream->_realPosInFile;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
    if(stream == NULL)
        return 0;
    if(stream->_endOfFile == 1)
        return 0;
    if(ptr == NULL || size == 0 || nmemb == 0)
    {
        stream->_errorFlag = TRUE;
        return SO_EOF;
    }

    size_t byteToRead = size * nmemb;
    size_t byteRead = 0;
    while (byteRead < byteToRead)
    {
        int chr = so_fgetc(stream);
        if(-1 == chr && stream->_errorFlag == 1)
            return 0;
        if(stream->_endOfFile == 1)
            return byteRead / size;

        *((unsigned char*)(char*)ptr + byteRead) = (unsigned char)chr;
        byteRead++;
    }
    
    stream->_lastOp = READ_FLAG;

    return byteRead / size;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
    if(stream == NULL || ptr == NULL)
        return SO_EOF;

    if(size == 0 || nmemb == 0)
        return 0;

    size_t bytesToWrite = size * nmemb;
    size_t bytesWritten = 0;

    while(bytesWritten < bytesToWrite)
    {
        int chr = *((char *)ptr + bytesWritten);
        int err = so_fputc(chr, stream);
        if(err == -1 && stream->_errorFlag)
            return 0;
        
        bytesWritten ++;
    }

    return bytesWritten / size;
}

int so_fgetc(SO_FILE *stream)
{
    if(stream == NULL)
        return SO_EOF;

    if(stream->_endOfFile == 1)
        return SO_EOF;

    if(stream->_lastOp == WRITE_FLAG)
    {
        int err = so_fflush(stream);
        if( -1 == err)
        {
            stream->_errorFlag = TRUE;
            return SO_EOF;
        }
        err = 0;
        err = SetFilePointer(
            stream->_fileDescriptor,
            0,
            NULL,
            FILE_BEGIN
        );
        if( -1 == err )
        {
            stream->_errorFlag == TRUE;
            return SO_EOF;
        }
        stream->_realPosInFile = 0;
    }

    if(stream->_bufferLen == stream->_posInBuffer)
    {   
        strcpy(stream->_buffer, "");
        stream->_bufferLen = 0;
        stream->_posInBuffer = 0;
        DWORD bytesRead;
        int readErr = ReadFile(
            stream->_fileDescriptor,
            stream->_buffer,
            BUFFER_SIZE,
            &bytesRead,
            NULL
        );

        if(!readErr)
        {
            stream->_errorFlag;
            return SO_EOF;
        }
        if(bytesRead == 0)
        {
            stream->_endOfFile = TRUE;
            return SO_EOF;
        }        

        stream->_bufferLen = bytesRead;
    }
    stream->_lastOp = READ_FLAG;
    stream->_posInBuffer++;
    stream->_realPosInFile++;

    return (int)stream->_buffer[stream->_posInBuffer - 1];
}

int so_fputc(int c, SO_FILE *stream)
{
    if(stream == NULL)
        return SO_EOF;

    if(stream->_lastOp == READ_FLAG )
    {
        DWORD offs = SetFilePointer(
            stream->_fileDescriptor,
            0,
            NULL,
            FILE_END
        );
        if(offs == INVALID_SET_FILE_POINTER)
        {
            stream->_errorFlag = TRUE;
            return SO_EOF;
        }
        stream->_realPosInFile = offs;

                strcpy(stream->_buffer, "");
        stream->_posInBuffer = 0;
        stream->_bufferLen = 0;
    }
    if(stream->_posInBuffer == BUFFER_SIZE)
    {
        int err = so_fflush(stream);
        if( -1 == err)
        {
            stream->_errorFlag = TRUE;
            return SO_EOF;
        }
    }

    stream->_bufferLen++;
    stream->_buffer[stream->_posInBuffer] = c;
    stream->_posInBuffer++;
    //stream->_realPosInFile++;

    stream->_lastOp = WRITE_FLAG;

    return c;
}

int so_feof(SO_FILE *stream)
{
    return stream->_endOfFile;
}

int so_ferror(SO_FILE *stream)
{
    return stream->_errorFlag;
}

SO_FILE *so_popen(const char *command, const char *type)
{
    return NULL;
}

int so_pclose(SO_FILE *stream)
{
    /* if(stream == NULL)
        return SO_EOF;

    int pid_to_wait = stream->pid;

    so_fclose(stream);

    int process_status;

    if(waitpid(pid_to_wait, &process_status, 0) == -1)
        return SO_EOF;

    return process_status; */
    return SO_EOF;
}
