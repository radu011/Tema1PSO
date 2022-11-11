#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "so_stdio.h"

#define BUFFER_SIZE 4096
#define READ_FLAG 1
#define WRITE_FLAG 2

// Code for linux SO
struct _so_file
{
    int _fileDescriptor;       // FIle descriptor number
    char _openMode[3];         // Opening mode
    int _openingFlags;         // Opening flags
    unsigned char _buffer[BUFFER_SIZE]; // Buffer for I/O
    int _bufferLen;            // Length of buffer
    int _posInBuffer;          // Cursor position in buffer
    long _posInFile;           // Cursor position in file (buffer)
    size_t _realPosInFile;       // Cursor position in file (real)
    int _endOfFile;            // End of file flag, set when cursor reached end of file
    int _errorFlag;            // Error flag, set when error appear -> non-zero value for error
    int _lastOp;               // Last operation executed on file
    int pid;
};

// Open file function
SO_FILE *so_fopen(const char *pathname, const char *mode)
{
    if(pathname == NULL || mode == NULL)
        return NULL;

    int oflag = 0;

    if (strcmp(mode, "r") == 0)
    {
        oflag = O_RDONLY;
    }
    else if (strcmp(mode, "r+") == 0)
    {
        oflag = O_RDWR;
    }
    else if (strcmp(mode, "w") == 0)
    {
        oflag = O_CREAT | O_TRUNC | O_WRONLY;
    }
    else if (strcmp(mode, "w+") == 0)
    {
        oflag = O_TRUNC | O_CREAT | O_RDWR;
    }
    else if (strcmp(mode, "a") == 0)
    {
        oflag = O_APPEND | O_CREAT | O_WRONLY;
    }
    else if (strcmp(mode, "a+") == 0)
    {
        oflag = O_APPEND | O_CREAT | O_RDWR;
    }
    else
    {
        return NULL;
    }

    int fd = open(pathname, oflag, 0644);

    if (-1 == fd)
    {
        return NULL;
    }

    SO_FILE *file = (SO_FILE *)malloc(sizeof(SO_FILE));
    if (file == NULL)
    {
        return NULL;
    }

    file->_fileDescriptor = fd;
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
        if(-1 == err)
        {
            stream->_errorFlag = 1;
            return SO_EOF;
        }
    }
    
    stream->_errorFlag = close(stream->_fileDescriptor);
    if (stream->_errorFlag)
    {
        free(stream);
        stream = NULL;    
        return SO_EOF;
    }

    free(stream);
    stream = NULL;
    return 0;
}

int so_fileno(SO_FILE *stream)
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
            bytesWritten = write(stream->_fileDescriptor, stream->_buffer + count, bytesToWrite - count);
            if (-1 == bytesWritten)
            {
                stream->_errorFlag = 1;
                return SO_EOF;
            }
            if(0 == bytesWritten)
            {
                stream->_endOfFile = 1;
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
            stream->_errorFlag = 1;
            return SO_EOF;
        }
    }
    else
    {
        strcpy(stream->_buffer, "");
        stream->_posInBuffer = 0;
        stream->_bufferLen = 0;
    }

    size_t offsetLocation = lseek(stream->_fileDescriptor, offset, whence);
    if (offsetLocation == -1)
    {
        stream->_errorFlag = 1;
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
            stream->_errorFlag = 1;
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
        stream->_errorFlag = 1;
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

        *((unsigned char*)(ptr + byteRead)) = (unsigned char)chr;
        byteRead++;
    }
    
    stream->_lastOp = READ_FLAG;

    return byteRead / size;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
    if(stream == NULL || ptr == NULL)
        return SO_EOF;

    size_t bytesToWrite = size * nmemb;
    size_t bytesWritten = 0;

    while(bytesWritten < bytesToWrite)
    {
        int err = so_fputc(*((char*)(ptr+bytesWritten)), stream);
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
            stream->_errorFlag = 1;
            return SO_EOF;
        }
        size_t val = lseek(stream->_fileDescriptor, 0, SEEK_SET);
        if(val == -1)
        {
            stream->_errorFlag = 1;
            return SO_EOF;
        }
        stream->_realPosInFile = 0;
    }

    if(stream->_bufferLen == stream->_posInBuffer)
    {   
        strcpy(stream->_buffer, "");
        stream->_bufferLen = 0;
        stream->_posInBuffer = 0;
        int readErr = read(stream->_fileDescriptor, stream->_buffer, BUFFER_SIZE);
        if( -1 == readErr )
        {
            stream->_errorFlag = 1;
            return SO_EOF;
        }
        if( 0 == readErr )
        {
            stream->_endOfFile = 1;
            return SO_EOF;
        }
        stream->_bufferLen = readErr;
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
        strcpy(stream->_buffer, "");
        stream->_posInBuffer = 0;
        stream->_bufferLen = 0;
    }
    if(stream->_posInBuffer == BUFFER_SIZE)
    {
        int err = so_fflush(stream);
        if( -1 == err)
        {
            stream->_errorFlag = 1;
            return SO_EOF;
        }
    }

    stream->_bufferLen++;
    stream->_buffer[stream->_posInBuffer] = c;
    stream->_posInBuffer++;
    stream->_realPosInFile++;

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
