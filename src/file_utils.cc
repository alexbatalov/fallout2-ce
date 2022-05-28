// NOTE: For unknown reason functions in this module use __stdcall instead
// of regular __usercall.

#include "file_utils.h"

#include <stdio.h>
#include <zlib.h>

#include <filesystem>

// 0x452740
int fileCopyDecompressed(const char* existingFilePath, const char* newFilePath)
{
    FILE* stream = fopen(existingFilePath, "rb");
    if (stream == NULL) {
        return -1;
    }

    int magic[2];
    magic[0] = fgetc(stream);
    magic[1] = fgetc(stream);
    fclose(stream);

    if (magic[0] == 0x1F && magic[1] == 0x8B) {
        gzFile inStream = gzopen(existingFilePath, "rb");
        FILE* outStream = fopen(newFilePath, "wb");

        if (inStream != NULL && outStream != NULL) {
            for (;;) {
                int ch = gzgetc(inStream);
                if (ch == -1) {
                    break;
                }

                fputc(ch, outStream);
            }

            gzclose(inStream);
            fclose(outStream);
        } else {
            if (inStream != NULL) {
                gzclose(inStream);
            }

            if (outStream != NULL) {
                fclose(outStream);
            }

            return -1;
        }
    } else {
        fileCopy(existingFilePath, newFilePath, true);
    }

    return 0;
}

// 0x452804
int fileCopyCompressed(const char* existingFilePath, const char* newFilePath)
{
    FILE* inStream = fopen(existingFilePath, "rb");
    if (inStream == NULL) {
        return -1;
    }

    int magic[2];
    magic[0] = fgetc(inStream);
    magic[1] = fgetc(inStream);
    rewind(inStream);

    if (magic[0] == 0x1F && magic[1] == 0x8B) {
        // Source file is already gzipped, there is no need to do anything
        // besides copying.
        fclose(inStream);
        fileCopy(existingFilePath, newFilePath, true);
    } else {
        gzFile outStream = gzopen(newFilePath, "wb");
        if (outStream == NULL) {
            fclose(inStream);
            return -1;
        }

        // Copy byte-by-byte.
        for (;;) {
            int ch = fgetc(inStream);
            if (ch == -1) {
                break;
            }

            gzputc(outStream, ch);
        }

        fclose(inStream);
        gzclose(outStream);
    }

    return 0;
}

// TODO: Check, implementation looks odd.
int _gzdecompress_file(const char* existingFilePath, const char* newFilePath)
{
    FILE* stream = fopen(existingFilePath, "rb");
    if (stream == NULL) {
        return -1;
    }

    int magic[2];
    magic[0] = fgetc(stream);
    magic[1] = fgetc(stream);
    fclose(stream);

    // TODO: Is it broken?
    if (magic[0] != 0x1F || magic[1] != 0x8B) {
        gzFile gzstream = gzopen(existingFilePath, "rb");
        if (gzstream == NULL) {
            return -1;
        }

        stream = fopen(newFilePath, "wb");
        if (stream == NULL) {
            gzclose(gzstream);
            return -1;
        }

        while (1) {
            int ch = gzgetc(gzstream);
            if (ch == -1) {
                break;
            }

            fputc(ch, stream);
        }

        gzclose(gzstream);
        fclose(stream);
    } else {
        fileCopy(existingFilePath, newFilePath, true);
    }

    return 0;
}

// Modelled as replacement for `CopyFileA`, but `overwrite` is the opposite to
// `bFailIfExists` param. Update callers accordingly.
void fileCopy(const char* existingFilePath, const char* newFilePath, bool overwrite)
{
    std::error_code ec;
    std::filesystem::copy_options options = overwrite
        ? std::filesystem::copy_options::overwrite_existing
        : std::filesystem::copy_options::none;
    std::filesystem::copy_file(std::filesystem::path(existingFilePath), std::filesystem::path(newFilePath), options, ec);
}
