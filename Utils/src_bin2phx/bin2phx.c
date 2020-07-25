/*******************************************************************************

BIN2PHX

RELEASED UNDER THE 3-CLAUSE BSD LICENSE:

COPYRIGHT 2019 MADONNA MARK III (SILICON SEX)

REDISTRIBUTION AND USE IN SOURCE AND BINARY FORMS, WITH OR WITHOUT MODIFICATION,
ARE PERMITTED PROVIDED THAT THE FOLLOWING CONDITIONS ARE MET:

1. REDISTRIBUTIONS OF SOURCE CODE MUST RETAIN THE ABOVE COPYRIGHT NOTICE, THIS
   LIST OF CONDITIONS AND THE FOLLOWING DISCLAIMER.

2. REDISTRIBUTIONS IN BINARY FORM MUST REPRODUCE THE ABOVE COPYRIGHT NOTICE,
   THIS LIST OF CONDITIONS AND THE FOLLOWING DISCLAIMER IN THE DOCUMENTATION
   AND/OR OTHER MATERIALS PROVIDED WITH THE DISTRIBUTION.

3. NEITHER THE NAME OF THE COPYRIGHT HOLDER NOR THE NAMES OF ITS CONTRIBUTORS
   MAY BE USED TO ENDORSE OR PROMOTE PRODUCTS DERIVED FROM THIS SOFTWARE WITHOUT
   SPECIFIC PRIOR WRITTEN PERMISSION.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

// INCLUDES ////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// DEFINES /////////////////////////////////////////////////////////////////////

#define SLOT_SIZE      (512 * 1024)
#define BITSTREAM_SIZE (460 * 1024)
#define EXTRA_SIZE     ( 48 * 1024)
#define SECTOR_SIZE    (  4 * 1024)
#define PAGE_SIZE      (       256)

#define HEADER_SIZE PAGE_SIZE
#define NAME_SIZE   (16)

// GLOBALS /////////////////////////////////////////////////////////////////////

struct
{
	char    name[NAME_SIZE];
	uint8_t issue[2];
	uint8_t reserved[HEADER_SIZE - (NAME_SIZE + 2)];
} header;

uint8_t sector[SECTOR_SIZE];

FILE *fi, *fo, *fx;

// CLEANUP /////////////////////////////////////////////////////////////////////

void cleanup(void)
{
	if (fx) fclose(fx);
	if (fo) fclose(fo);
	if (fi) fclose(fi);
}

// ABEND ///////////////////////////////////////////////////////////////////////

void abend(char *error)
{
	fprintf(stderr, "\aERROR: %s\n", error);
	cleanup();
	exit(EXIT_FAILURE);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
//	BANNER

	printf("BIN2PHX 1.2\n");
	printf("PHOENIX PHX CORE FILE MASTERING\n");
	printf("DEEP-FRIED BY MADONNA MARK III\n");
	printf("COPYRIGHT 2019 SILICON SEX\n\n");

//	ARGUMENTS

	if ((argc != 5) && (argc != 6)) abend("WRONG NUMBER OF ARGUMENTS");

	if (!(fi = fopen(argv[1], "rb"))) abend("CANNOT OPEN SOURCE FILE");
	if (!(fo = fopen(argv[2], "wb"))) abend("CANNOT OPEN TARGET FILE");

//	HEADER: NAME

	memset(&header, 0, sizeof header);
	memset(header.name, ' ', NAME_SIZE);

	size_t l = strlen(argv[3]);

	if ((l == 0) || (l > NAME_SIZE)) abend("INVALID NAME LENGTH");

	for (size_t i = 0; i < l; i++)
	{
		char c = argv[3][i];
		if
		(
			((c >= '0') && (c <= '9')) ||
			((c >= 'A') && (c <= 'Z')) ||
			 (c == ' ') || (c == '-')
		)
			header.name[i] = c;
		else
			abend("INVALID CHARACTER IN NAME");
	}

//	HEADER: ISSUE

	int32_t issue = atol(argv[4]);

	if ((issue < 0) || (issue > 65535)) abend("ISSUE OUT OF RANGE");

	header.issue[0] = ((uint16_t)issue) & 0xFF;
	header.issue[1] = ((uint16_t)issue) >> 8;

//	BITSTREAM

	for (size_t i = 0; i < BITSTREAM_SIZE / SECTOR_SIZE; i++)
	{
		memset(sector, 0, SECTOR_SIZE);

		if (i == 0)
		{
			memcpy(sector, &header, HEADER_SIZE);

			if (!fread(sector + HEADER_SIZE, SECTOR_SIZE - HEADER_SIZE, 1, fi))
				if (ferror(fi)) abend("SOURCE FILE READ ERROR");
			if (!fwrite(sector, SECTOR_SIZE, 1, fo))
				abend("TARGET FILE HEADER WRITE ERROR");
		}
		else
		{
			if (!fread(sector, SECTOR_SIZE, 1, fi))
				if (ferror(fi)) abend("SOURCE FILE READ ERROR");
			if (!fwrite(sector, SECTOR_SIZE, 1, fo))
				abend("TARGET FILE BITSTREAM WRITE ERROR");
		}
	}

//	EXTRA DATA (OPTIONAL)

	if (argc == 6)
	{
		if (!(fx = fopen(argv[5], "rb"))) abend("CANNOT OPEN EXTRA DATA FILE");

		for (size_t i = 0; (i < EXTRA_SIZE / SECTOR_SIZE) && !feof(fx); i++)
		{
			memset(&sector, 0, SECTOR_SIZE);

			if (!fread(sector, SECTOR_SIZE, 1, fx))
				if (ferror(fx)) abend("EXTRA DATA FILE READ ERROR");
			if (!fwrite(sector, SECTOR_SIZE, 1, fo))
				abend("TARGET FILE EXTRA DATA WRITE ERROR");
		}
	}

//	CLEANUP

	cleanup();
	return EXIT_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////