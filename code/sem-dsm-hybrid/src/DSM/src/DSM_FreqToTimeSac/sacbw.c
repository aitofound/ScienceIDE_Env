#include <stdio.h>
#include <string.h>
#include <errno.h>

int sacbw_( int *mfhdr, int *mnhdr, int *mihdr, int *mlhdr, int *mkhdr, float fhdr[], int nhdr[], int ihdr[], int lhdr[], char khdr[], int *npts, float data[], char filen[] )
{
	FILE *file;

	file = fopen( filen,"w" );
	if (file == NULL) {
		fprintf(stderr, "sacbw: cannot open '%s': %s\n", filen, strerror(errno));
		return(1);
	}
	fwrite( fhdr,4,*mfhdr,file );
	fwrite( nhdr,4,*mnhdr,file );
	fwrite( ihdr,4,*mihdr,file );
	fwrite( lhdr,4,*mlhdr,file );
	fwrite( khdr,8,*mkhdr,file );
	fwrite( data,4,*npts,file );
	fclose(file);
	return(0);
}

int filenchk_( int *nchar, char filen[] )
{
	int i;

	for (i=0;i<*nchar;i=i+1) {
	  if ( filen[i]==' ' ) filen[i]=0;
	}
	return(0);
}
