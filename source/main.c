#include <stdio.h>
#include <stdlib.h>
#include <3ds.h>
#include <stdarg.h>
#include <dirent.h>


#include "lodepng.h"
#include "actu.h"
#include "ext.h"
#include "ConvertUTF.h"

int print2(char *format, ...)
{
    va_list args;
    va_start(args, format);
	int ret = vprintf(format, args);
    va_end(args);
	gfxFlushBuffers();
	gfxSwapBuffers();
	return ret;
}

void drawPixel(u8* framebuffer, int x, int y, int r, int g, int b) {

	if (x > 0 && x < 400 && y > 0 && y < 240) {
		int index = 3*((239-y)+x*240);
		framebuffer[index] = b;
		framebuffer[index+1] = g;
		framebuffer[index+2] = r;
	}
}

int pngToRGB565(char* filename, u16* rgb_buf_64x64, u8* alpha_buf_64x64, u16* rgb_buf_32x32, u8* alpha_buf_32x32) {

	int ret = 0;
	u8* image;
	unsigned width, height;
	
	ret = lodepng_decode32_file(&image, &width, &height, filename);
	if (ret) {print2("error %u: %s\n", ret, lodepng_error_text(ret)); return ret;}
	
	if (width != 64 || height != 64) {print2("wrong size, should be 64x64\n"); ret=-1; goto end;}
	
	print2("success!\n");
	
	int x, y, r, g, b, a;
	
	int rand_x = rand() % (400-64);
	int rand_y = rand() % (240-64);
	
	memset(alpha_buf_64x64, 0, 64*64/2);
	memset(alpha_buf_32x32, 0, 32*32/2);
	
	u8* framebuffer = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	
	for (y=0; y<64; ++y) {
		for (x=0; x<64; ++x) {
			r = image[y*64*4 + x*4] >> 3;
			g = image[y*64*4 + x*4 + 1] >> 2;
			b = image[y*64*4 + x*4 + 2] >> 3;
			a = image[y*64*4 + x*4 + 3] >> 4;
			
			int rgb565_index = 8*64*(y/8) | 64*(x/8) | 32*((y/4)%2) | 16*((x/4)%2) | 8*((y/2)%2) | 4*((x/2)%2) | 2*(y%2) | (x%2);
			
			rgb_buf_64x64[rgb565_index] = (r << 11) | (g << 5) | b;
			alpha_buf_64x64[rgb565_index / 2] |= a << (4*(x%2));
			
			if (a) drawPixel(framebuffer, rand_x+x, rand_y+y, image[y*64*4 + x*4], image[y*64*4 + x*4 + 1], image[y*64*4 + x*4 + 2]);
		}
	}

	for (y=0; y<64; y+=2){
		for (x=0; x<64; x+=2){
			r = (image[y*64*4 + x*4 + 0] + image[(y+1)*64*4 + x*4 + 0] + image[y*64*4 + (x+1)*4 + 0] + image[(y+1)*64*4 + (x+1)*4 + 0]) >> 5;
			g = (image[y*64*4 + x*4 + 1] + image[(y+1)*64*4 + x*4 + 1] + image[y*64*4 + (x+1)*4 + 1] + image[(y+1)*64*4 + (x+1)*4 + 1]) >> 4;
			b = (image[y*64*4 + x*4 + 2] + image[(y+1)*64*4 + x*4 + 2] + image[y*64*4 + (x+1)*4 + 2] + image[(y+1)*64*4 + (x+1)*4 + 2]) >> 5;
			a = (image[y*64*4 + x*4 + 3] + image[(y+1)*64*4 + x*4 + 3] + image[y*64*4 + (x+1)*4 + 3] + image[(y+1)*64*4 + (x+1)*4 + 3]) >> 6;
			
			int halfx = x/2;
			int halfy = y/2;
			
			int rgb565_index = 4*64*(halfy/8) | 64*(halfx/8) | 32*((halfy/4)%2) | 16*((halfx/4)%2) | 8*((halfy/2)%2) | 4*((halfx/2)%2) | 2*(halfy%2) | (halfx%2);
			
			rgb_buf_32x32[rgb565_index] = (r << 11) | (g << 5) | b;
			alpha_buf_32x32[rgb565_index / 2] |= a << (4*(halfx%2));
		}
	}
	
	end:
	free(image);
	return ret;
}

void charToUnicode(u16* dst, char* src) {
	if(!src || !dst)return;
	while(*src)*(dst++)=(*(src++));
	*dst='\0';
}

int setupExtdata() {

	u32 extdata_archive_lowpathdata[3] = {mediatype_SDMC, 0x000014d1, 0};
	FS_archive extdata_archive = (FS_archive){ARCH_EXTDATA, (FS_path){PATH_BINARY, 0xC, (u8*)extdata_archive_lowpathdata}};
	
	Result ret = FSUSER_OpenArchive(NULL, &extdata_archive);
	FSUSER_CloseArchive(NULL, &extdata_archive);
	
	if (ret==0) {
		print2("Extdata exists.\n");
		return 0;
		
	} else {
	
		print2("Creating ExtSaveData...\n");
		ret = CreateExtSaveData(0x14d1);
		if (ret) print2("CreateExtSaveData failed! %08x\n", ret);
		return ret;
	}
}

u64 getShortcut(char *filename) {

	u64 shortcut = 0xFFFFFFFFFFFFFFFF;
	
	char *p1;
	for (p1=filename; *p1 != '.' && *p1 != '\0'; ++p1) {;}
	
	if (*p1 != '.') return shortcut;
	++p1;
	
	char *p2;
	for (p2=p1; *p2 != '.' && *p2 != '\0'; ++p2) {;}
	
	if (*p2 != '.') return shortcut;
	if (p2-p1 != 8) return shortcut;
	
	unsigned int lowpath;
	
	if (sscanf(p1, "%08x", &lowpath) != 1) return shortcut;
	
	shortcut = 0x0004001000000000 + lowpath;
	return shortcut;
}

int writeToExtdata(int nnidNum) {
	
	u32 extdata_archive_lowpathdata[3] = {mediatype_SDMC, 0x000014d1, 0};
	FS_archive extdata_archive = (FS_archive){ARCH_EXTDATA, (FS_path){PATH_BINARY, 0xC, (u8*)extdata_archive_lowpathdata}};
	Handle filehandle;
	u32 tmpval=0;
	u64 badgeDataSize = 0xF4DF80;
	u64 badgeMngSize = 0xD4A8;
	Result ret = 0;
	
	u16 rgb_buf_64x64[64*64];
	u8 alpha_buf_64x64[64*64/2];
	u16 rgb_buf_32x32[32*32];
	u8 alpha_buf_32x32[32*32/2];
	
	DIR *dir;
	struct dirent *ent;
	dir = opendir("badges");
	if (dir == NULL) return -1;
	
	u8 *badgeDataBuffer = malloc(badgeDataSize);
	if (badgeDataBuffer == NULL) {closedir(dir); return -2;}
	memset(badgeDataBuffer, 0, badgeDataSize);
	
	u8 *badgeMngBuffer = malloc(badgeMngSize);
	if (badgeMngBuffer == NULL) {closedir(dir); free(badgeDataBuffer); return -2;}
	memset(badgeMngBuffer, 0, badgeMngSize);
	
	int badge_count = 0;
	int i;
	
	while ((ent = readdir(dir)) != NULL) {
		print2("trying to read png...\n");
		char path[0x1000];
		sprintf(path, "badges/%s", ent->d_name);
		print2("%s\n", path);
		
		u16 utf16_name[0x8A/2];
		ret = ConvertUTF8toUTF16((const UTF8 *) ent->d_name, utf16_name, 0x8A/2);
		
		u16* p;
		for (p=utf16_name; *p != '.' && *p != '\0'; ++p) {;}
		*p = '\0';
		
		u64 shortcut = getShortcut(ent->d_name);
		
		ret = pngToRGB565(path, rgb_buf_64x64, alpha_buf_64x64, rgb_buf_32x32, alpha_buf_32x32);
		if (ret == 0) {
			for (i=0; i<16; ++i) {
				memcpy(badgeDataBuffer + 0x35E80 + badge_count*16*0x8A + i*0x8A, utf16_name, 0x8A);
			}
			memcpy(badgeDataBuffer + 0x318F80 + badge_count*0x2800, rgb_buf_64x64, 64*64*2);
			memcpy(badgeDataBuffer + 0x31AF80 + badge_count*0x2800, alpha_buf_64x64, 64*64/2);
			memcpy(badgeDataBuffer + 0xCDCF80 + badge_count*0xA00, rgb_buf_32x32, 32*32*2);
			memcpy(badgeDataBuffer + 0xCDD780 + badge_count*0xA00, alpha_buf_32x32, 32*32/2);
			
			int badge_id = badge_count+1;
			memcpy(badgeMngBuffer + 0x3E8 + badge_count*0x28 + 0x4, &badge_id, 4);
			badgeMngBuffer[0x3E8 + badge_count*0x28 + 0x8] = 0xBE;
			badgeMngBuffer[0x3E8 + badge_count*0x28 + 0x9] = 0xEF;
			memcpy(badgeMngBuffer + 0x3E8 + badge_count*0x28 + 0xC, &badge_count, 2);
			badgeMngBuffer[0x3E8 + badge_count*0x28 + 0x12] = 255;
			badgeMngBuffer[0x3E8 + badge_count*0x28 + 0x13] = 255;
			
			memcpy(badgeMngBuffer + 0x3E8 + badge_count*0x28 + 0x18, &shortcut, 8);
			memcpy(badgeMngBuffer + 0x3E8 + badge_count*0x28 + 0x20, &shortcut, 8);
			
			badgeMngBuffer[0x358 + badge_count/8] |= 1 << (badge_count % 8);
		
			++badge_count;
		}
	}
	
	print2("Attempting to write extdata...\n");
	
	ret = FSUSER_OpenArchive(NULL, &extdata_archive);
	if (ret != 0) {print2("FSUSER_OpenArchive failed! %08x\n", ret); goto end;}

	FS_path path = FS_makePath(PATH_CHAR, "/BadgeData.dat");
	
	ret = FSUSER_CreateFile(NULL, extdata_archive, path, badgeDataSize);
	ret = FSUSER_OpenFile(NULL, &filehandle, extdata_archive, path, FS_OPEN_WRITE, 0);
	if (ret != 0) {print2("FSUSER_OpenFile failed! %08x\n", ret); goto end;}
	ret = FSFILE_Write(filehandle, &tmpval, 0, badgeDataBuffer, badgeDataSize, FS_WRITE_FLUSH);
	ret = FSFILE_Close(filehandle);
	
	u32 total_badges = 0xFFFF * badge_count;
	
	badgeMngBuffer[0x04] = 0;
	memcpy(badgeMngBuffer + 0x8, &badge_count, 4);
	badgeMngBuffer[0x10] = 0xFF;
	badgeMngBuffer[0x11] = 0xFF;
	badgeMngBuffer[0x12] = 0xFF;
	badgeMngBuffer[0x13] = 0xFF;
	memcpy(badgeMngBuffer + 0x18, &total_badges, 4);
	memcpy(badgeMngBuffer + 0x1C, &nnidNum, 4);
	badgeMngBuffer[0x3D8] = 1;
	
	badgeMngBuffer[0xA028 + 0x0] = 0xFF;
	badgeMngBuffer[0xA028 + 0x1] = 0xFF;
	badgeMngBuffer[0xA028 + 0x2] = 0xFF;
	badgeMngBuffer[0xA028 + 0x3] = 0xFF;
	badgeMngBuffer[0xA028 + 0x4] = 0xFF;
	badgeMngBuffer[0xA028 + 0x5] = 0xFF;
	badgeMngBuffer[0xA028 + 0x6] = 0xFF;
	badgeMngBuffer[0xA028 + 0x7] = 0xFF;
	badgeMngBuffer[0xA028 + 0xC] = 0x10;
	badgeMngBuffer[0xA028 + 0xD] = 0x27;
	badgeMngBuffer[0xA028 + 0x10] = 0xBE;
	badgeMngBuffer[0xA028 + 0x11] = 0xEF;
	badgeMngBuffer[0xA028 + 0x18] = 0xFF;
	badgeMngBuffer[0xA028 + 0x19] = 0xFF;
	badgeMngBuffer[0xA028 + 0x1A] = 0xFF;
	badgeMngBuffer[0xA028 + 0x1B] = 0xFF;
	memcpy(badgeMngBuffer + 0xA028 + 0x1C, &badge_count, 4);
	memcpy(badgeMngBuffer + 0xA028 + 0x20, &total_badges, 4);

	path = FS_makePath(PATH_CHAR, "/BadgeMngFile.dat");
	
	ret = FSUSER_CreateFile(NULL, extdata_archive, path, badgeMngSize);
	ret = FSUSER_OpenFile(NULL, &filehandle, extdata_archive, path, FS_OPEN_WRITE, 0);
	if (ret != 0) {print2("FSUSER_OpenFile failed! %08x\n", ret); goto end;}
	ret = FSFILE_Write(filehandle, &tmpval, 0, badgeMngBuffer, badgeMngSize, FS_WRITE_FLUSH);
	ret = FSFILE_Close(filehandle);
	
	end:
	FSUSER_CloseArchive(NULL, &extdata_archive);
	closedir(dir);
	free(badgeDataBuffer);
	free(badgeMngBuffer);
	return ret;
}

int main() {

	gfxInitDefault();
	gfxSetDoubleBuffering(GFX_TOP, false);
	consoleInit(GFX_BOTTOM, NULL);
	srand(time(NULL));
	Result ret;
	
	setupExtdata();
	
    u32 nnidNum = 0;
    ret = actInit();
    ret = ACTU_Initialize(0xB0002C8, 0, 0);
    ret = ACTU_GetAccountDataBlock(0xFE, 4, 12, &nnidNum);
    ret = actExit();

	if (nnidNum) {
		print2("nnid: %08x\n", (int) nnidNum);
	} else {
		print2("error, could not detect NNID!");
	}
	
	ret = writeToExtdata(nnidNum);
	if (ret == 0xC92044E6) {
		print2("----File in use. Please load all badges in your badge case before launching.----\n");
		svcSleepThread(5000000000LL);
	}
	else if (ret == 0) {
		print2("Successfully!\n");
	} else {
		print2("WHAT IS WRONG WITH THE ELF. %08x\n", ret);
	}
	
	
	svcSleepThread(5000000000LL);

	gfxExit();
	return 0;
}