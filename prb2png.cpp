// prb2png.cpp : Defines the entry point for the console application.
//

#include "lodepng.h"
#include "ConvertUTF.h"

using namespace std;

void removeInvalidChars(char *str) {

	char *p;
	for (p = str; *p != '\0'; ++p) {
		if (*p == '.' || (*p > 0 && *p < 0x20) || *p == ':') {
			*p = ' ';
		}
	}
}

string getFileName(const string& s) {

	char sep = '/';

#ifdef _WIN32
	sep = '\\';
#endif

	size_t i = s.rfind(sep, s.length());
	if (i != string::npos) {
		return(s.substr(i + 1, s.length() - i));
	}

	return("");
}

long getFileSize(FILE *file)

{
	long lCurPos, lEndPos;
	lCurPos = ftell(file);
	fseek(file, 0, 2);
	lEndPos = ftell(file);
	fseek(file, lCurPos, 0);

	return lEndPos;

}


/* See GYTB */
int rgb565ToPng(char* filename, uint16_t* rgb_buf, uint8_t* alpha_buf) {

	uint8_t* image = (uint8_t*)malloc(64 * 64 * 4);
	if (!image) return -1;

	int i, x, y, r, g, b, a;
	
	for (i = 0; i < 64 * 64; ++i) {
		r = (rgb_buf[i] & 0xF800) >> 11;
		g = (rgb_buf[i] & 0x07E0) >> 5;
		b = (rgb_buf[i] & 0x001F);
		a = (alpha_buf[i / 2] >> (4 * (i % 2))) & 0x0F;

		r = round(r * 255.0 / 31.0);
		g = round(g * 255.0 / 63.0);
		b = round(b * 255.0 / 31.0);
		a = a * 0x11;

		x = 8 * ((i / 64) % 8) + (((i % 64) & 0x01) >> 0) + (((i % 64) & 0x04) >> 1) + (((i % 64) & 0x10) >> 2);
		y = 8 * (i / 512) + (((i % 64) & 0x02) >> 1) + (((i % 64) & 0x08) >> 2) + (((i % 64) & 0x20) >> 3);


		image[y * 64 * 4 + x * 4 + 0] = r;
		image[y * 64 * 4 + x * 4 + 1] = g;
		image[y * 64 * 4 + x * 4 + 2] = b;
		image[y * 64 * 4 + x * 4 + 3] = a;
	} 

	int ret = lodepng_encode32_file(filename, image, 64, 64);
	
	free(image);
	return ret;
}



int main(int argc, char* argv[])
{
	unsigned char * fileBuf;
	FILE * prb;
	//char * seriesNameBuffer = new char [0x30];
	char * badgeNameBuffer = new char[0x30];

	/* Set Up - open the file and copy it to a buffer. */
	fopen_s(&prb, argv[1], "rb");


	string filename = getFileName(argv[1]);

	
	long fileSize = getFileSize(prb);

	fileBuf = new unsigned char[fileSize];

	fread(fileBuf, fileSize, 1, prb);
	
	fseek(prb, 0x44, SEEK_SET);
	fread(badgeNameBuffer, 0x30, 1, prb);
	
	fseek(prb, 0x74, SEEK_SET);
	//fread(seriesNameBuffer, 0x30, 1, prb);

	//uint16_t* utf16_name = (uint16_t*)(fileBuf+ + 0xE0);
	//char utf8_name[256] = "";
	//ConvertUTF16toUTF8(utf16_name, (UTF8*)utf8_name, 256);

	//removeInvalidChars(utf8_name);
	//removeInvalidChars(seriesNameBuffer);
	removeInvalidChars(badgeNameBuffer);

	//string seriesName = seriesNameBuffer;
	string badgeName = badgeNameBuffer;
	//string collectionName = utf8_name;

	/* Processing Mega Badges

	   Okay some prb files contain mega badges. These are two
	   and four part badges that make up one overall image. The
	   images are stored sequentially in the file.

	   Interestingly, each prb with a mega badge also contains
	   a 64x64 picture of the entire badge put together. It's
	   always in the first position in the file. This program
	   skips generating this downsampled file - it only generates
	   the parts.
	   
	   The logic below determines how many badges are in each file.

	   The first conditional handles prb files where the data size 
	   is exactly a multiple of 0x3200. The number of images in v130 
	   prb files are correctly detected by a mod of filesize to 0x3200.
	   
	     0x3200 for one-image badges
	     0x9600 for two-image badges  (the first is a downscaled image, the next TWO are the full fidelity images)
	     0xFA00 for four-image badges (the first is a downscaled image, the next FOUR are the full fidelity images)
	
	   After that are some hacks. In the v131 version of prb files, 
	   there is unknown data after the last image so right now I've 
	   done something really ugly to achieve determining the number
	   of images in a prb. 
	*/

	// Header is 0x1100 bytes
	int prbdatasize = (fileSize - 0x1100);
	int badges = 0;

	if (prbdatasize % 0x3200 == 0) {
		badges = (prbdatasize) / (0x2800 + 0xA00);
	}
	// hacks until we figure out what's at the end of the file
	// basically determine the number if images by file size. gross.
	else if (prbdatasize > 90000) {
		badges = 5;
	}
	else if (prbdatasize > 70000) { 
		badges = 3;
	}
	else {
		badges = 1;
	}

	/* Processing prb files
	   
	   PRB file layout is as follows:
	   0x0000-0x10FF : Header. Title is contained here, but right now we don't do anything with it.
	   0x1100-0x30FF : Image RGB data (0x2000)
	   0x3100-0x38FF : Image Alpha data (0x800)
	   0x3900-0x42FF : Image preview data stream. I think it's split into 0x800 rgb and 0x200 alpha. (0xA00)
	   
	   For mega prbs, subsequent images start at offsets of (0x2800 + 0xA00) from the previous.
	
	*/


	for (int i = 0; i < badges; i++) {
		
		
		// skip the first image in mega prbs
		if (badges > 1 && i==0)
			continue; 

		// place rgb and alpha data streams into separate buffers
		uint16_t* rgb_ptr = (uint16_t*)(fileBuf + 0x1100 + (i * (0x2800+0xA00)));
		uint8_t* alpha_ptr = (uint8_t*)fileBuf + 0x3100 + (i * (0x2800+0xA00));

		string suffix = "";
		if (badges > 1) {
			suffix = ".mega" + to_string(i);	
		}
		
		string name_with_ext = (badgeName + suffix +  ".png").c_str();
		rgb565ToPng((char *)name_with_ext.c_str(), rgb_ptr, alpha_ptr);
		
	}
	return 0;
}
