#include    <stdio.h>

#ifdef NO_POPEN_PROTOTYPE
    /*
     *    This is supposed to be found in stdio.h.
     */

    FILE    *popen(char *popen_command, const char *type);
    int        pclose(FILE *stream);
#endif

#include    <stdlib.h>
#include    <string.h>
#include    "cbf.h"
#include    "cbf_string.h"

/****************************************************************/

static void gethd ( char* field, char* value, char* header )
{
  char *hp, *lhp, *fp, *vp;
  int l, j;
  char *newfield;

  /*
   * Find the last occurance of "field" in "header"
   */
  l = strlen (field);
  newfield = (char*) malloc ( l + 3 );
  *newfield = 10;
  strncpy (newfield+1, field, l);
  *(newfield+l+1) = '=';
  *(newfield+l+2) = (char) 0;
  l += 2;

  lhp = 0;
  for (hp=header; *hp != '}'; hp++)
    {
      for (fp=newfield, j=0; j<l && *(hp+j) == *fp; fp++, j++);
      if ( j == l ) lhp=hp;
    }

  if ( lhp == 0 )
    value[0] = 0;
  else
    {
      /*
       * Find the '='.  hp will now point to the '='
       */
      for (hp=lhp; *hp!='='; hp++);
      
      /*
       * Copy into the returned value until the ';'
       */
      for (lhp=hp+1, vp=value; *lhp!=';' && *lhp!=0; lhp++, vp++) *vp = *lhp;
      *(vp++)=0;
    }
  free (newfield);
}

static int    endswith(char *haystack, char *needle)
{
    char    *cp;

    if(NULL == (cp = (char *) strstr(haystack, needle)))
        return(0);
    if('\0' == *(cp + strlen(needle)))
        return(1);
    return(0);
}

static void usage( void )
{
        fprintf(stderr,"Usage: adscimg2cbf [--flag[,modifier]] file1.img ... filen.img     (creates file1.cbf ... filen.cbf)\n");
        fprintf(stderr,"       Image files may also be compressed (.gz, .bz2, .Z)\n");
        fprintf(stderr,"Flags:\n");
        fprintf(stderr,"\t--cbf_byte_offset        Use BYTE_OFFSET compression (DEFAULT).\n");
        fprintf(stderr,"\t--cbf_packed            Use CCP4 packing (JPA) compression.\n");
        fprintf(stderr,"\t--cbf_packed_v2        Use CCP4 packing version 2 (JPA) compression.\n");
        fprintf(stderr,"\t--no_compression        No compression.\n\n");
        fprintf(stderr,"The following two modifiers can be appended to the flags (syntax: --flag,modifier):\n");
        fprintf(stderr,"\t,flat                Flat (linear) images.\n");
        fprintf(stderr,"\t,uncorrelated            Uncorrelated sections.\n\n");
        fprintf(stderr,"The following describe how to interpret the beam center in the ADSC header:\n");
        fprintf(stderr,"\t--beam_center_from_header    Figure out beam center from ADSC header information (default)\n");
        fprintf(stderr,"\t--beam_center_mosflm        Beam center in ADSC header: MOSFLM coordinates.\n");
        fprintf(stderr,"\t--beam_center_ulhc        Beam center in ADSC header: origin: upper left hand corner of image.(HKL mm)\n");
        fprintf(stderr,"\t--beam_center_llhc        Beam center in ADSC header: origin: lower left hand corner of image.(adxv mm)\n");
    fprintf(stderr,"\t--region-of-interest=fastlow,fasthigh,slowlow,slowhigh\n");
        fprintf(stderr,"\t                              Region of interest to map to CBF\n");
    fprintf(stderr,"\t--sensor-thickness=0.00000000\n");
    fprintf(stderr,"\t                          Sensor layer thickness in mm\n");
    fprintf(stderr,"\t--add-minicbf-header      Add a minicbf header for fast_dp\n");
    fprintf(stderr,"\t--polarization-source-ratio=ratio\n");
    fprintf(stderr,"\t                          Polarization source ratio (Ip-In)/(Ip+In)\n");
    fprintf(stderr,"\t--scale=1.0              Scale the data\n");
    fprintf(stderr,"\t--offset=0.0             Offset the data\n");
    fprintf(stderr,"\t--cliplow=0.0            Clip the scaled and offset data below\n");
    fprintf(stderr,"\t--cliphigh=16384.0       Clip the scaled and offset deata above\n");
    fprintf(stderr,"\t--radial-smooth=nn       Radially smooth over nn pixels\n");

    
    
}

/*
 *    Jiffy to take one or more adsc .img files, convert each in turn to
 *    .cbf files, and output the files.  The input file is preserved; the
 *    output file has the same name as the input file with the suffix .img
 *    replaced by .cbf.
 */

char    popen_command[1080];

#define BEAM_CENTER_FROM_HEADER 0
#define BEAM_CENTER_MOSFLM      1
#define BEAM_CENTER_ULHC        2
#define BEAM_CENTER_LLHC        3

int adscimg2cbf_sub2(char *header,
                     unsigned short *data,
                     char *cbf_filename,
                     int pack_flags,
                     int beam_center_convention,
                     int pad_flag,
                     const char *roi,
                     const char *thickstr,
                     int add_mini_cbf_header,
                     const char *polarstr,
                     int bin,
                     const char *scalestr,
                     const char *offsetstr,
                     const char *cliplowstr,
                     const char *cliphighstr,
                     const char *rad_smoothstr);


int    main(int argc, char *argv[])
{
    FILE        *fp;
    char        in_filename[1024], out_filename[1024];
    char        field[128];
    char        *hptr;
    unsigned short    *uptr;
    char        header_bytes[6];
    int        file_size, header_size_char, actread;
    int        cbf_status;
    int        i, j, k=0;
    int        size1, size2;
    int        file_type;
    int        pack_flags;
    int        pad_flag;
    int        status_pclose;
    int        beam_center_convention;
    int     add_minicbf_header=0;
    int     bin=0;
    double  thickness=0.;
    const char *    roi=NULL;
    const char *    thickstr=NULL;
    const char *    polarstr=NULL;
    const char *    scalestr=NULL;
    const char *    offsetstr=NULL;
    const char *    cliplowstr=NULL;
    const char *    cliphighstr=NULL;
    const char *    rad_smoothstr=NULL;
    char *     strstrhit;
    static char    *endings[] = {
                    ".img",
                    ".img.gz",
                    ".img.bz2",
                    ".img.Z",
                    NULL
                     };
	static char	*flags[28] = {
                    "--cbf_byte_offset",            /* 0 */
                    "--cbf_packed_v2",              /* 1 */
                    "--cbf_packed",                 /* 2 */
                    "--no_compression",             /* 3 */
                    "--beam_center_from_header",    /* 4 */
                    "--beam_center_mosflm",         /* 5 */
                    "--beam_center_ulhc",           /* 6 */
                    "--beam_center_llhc",           /* 7 */
                    "--pad_1K",                     /* 8 */
                    "--pad_2K",                     /* 9 */
                    "--pad_4K",                     /*10 */
                    "--no_pad",                     /*11 */
                    "--cbf_nibble_offset",          /*12 */
                    "--region-of-interest",         /*13 */
                    "--region_of_interest",         /*14 */
                    "--sensor-thickness",           /*15 */
                    "--sensor_thickness",           /*16 */
                    "--add-minicbf-header",         /*17 */
                    "--add_minicbf_header",         /*18 */
                    "--polarization-source-ratio",  /*19 */
                    "--polarization_source_ratio",  /*20 */
                    "--bin",                        /*21 */
                    "--scale",                      /*22 */
                    "--offset",                     /*23 */
                    "--cliplow",                    /*24 */
                    "--cliphigh",                   /*25 */
                    "--radial-smooth",              /*26 */
                    NULL
                   };


    if(argc < 2)
    {
        usage();
        exit(0);
    }

    pack_flags = CBF_BYTE_OFFSET;
    beam_center_convention = BEAM_CENTER_FROM_HEADER;
    pad_flag = PAD_4K;

    while(argc > 1 && argv[1][0] == '-' && argv[1][1] == '-')
    {
        for(j = 0; flags[j] != NULL; j++) {
        strstrhit = cbf_cistrnstr(argv[1], flags[j], strlen(flags[j]));
            if(NULL != strstrhit && strstrhit==argv[1] )
                break;
    }
        if(NULL == flags[j])
        {
            fprintf(stderr,"adscimg2cbf: %s is an unknown flag\n\n", argv[1]);
            usage();
            exit(0);
        }
        switch(j)
        {
            case 0:
                pack_flags = CBF_BYTE_OFFSET;
                break;
            case 1:
                pack_flags = CBF_PACKED_V2;
                break;
            case 2:
                pack_flags = CBF_PACKED;
                break;
            case 3:
                pack_flags = CBF_NONE;
                break;
            case 4:
                beam_center_convention = BEAM_CENTER_FROM_HEADER;
                break;
            case 5:
                beam_center_convention = BEAM_CENTER_MOSFLM;
                break;
            case 6:
                beam_center_convention = BEAM_CENTER_ULHC;
                break;
            case 7:
                beam_center_convention = BEAM_CENTER_LLHC;
                break;
            case 8:
                pad_flag = PAD_1K;
                break;
            case 9:
                pad_flag = PAD_2K;
                break;
            case 10:
                pad_flag = PAD_4K;
                break;
            case 11:
                pad_flag = 0;
                break;
            case 12:
                pack_flags = CBF_NIBBLE_OFFSET;
                break;
            case 13:
            case 14:
                roi = argv[1]+strlen(flags[j])+1;
                if (*(roi-1)!='='||strlen(roi)<3){
                    fprintf(stderr,"adscimg2cbf: %s should be "
                            "--region-of-interest=fastlow,fasthigh,slowlow,slowhigh\n\n", argv[1]);
                    usage();
                    exit(0);
                }
                break;
            case 15:
            case 16:
                thickstr = argv[1]+strlen(flags[j])+1;
                if (*(thickstr-1)!='='||strlen(thickstr)<3){
                    fprintf(stderr,"adscimg2cbf: %s should be "
                            "--sensor-thickness=thickness\n\n", argv[1]);
                    usage();
                    exit(0);
        }
                break;
            case 17:
            case 18:
                add_minicbf_header=1;
                break;
            case 19:
            case 20:
                polarstr = argv[1]+strlen(flags[j])+1;
                if (*(polarstr-1)!='='||strlen(polarstr)<3){
                    fprintf(stderr,"adscimg2cbf: %s should be "
                            "--polarization-source-ratio=polarization\n\n", argv[1]);
                    usage();
                    exit(0);
                }
                break;
            case 21:
                bin = 1;
                break;
            case 22:
                scalestr = argv[1]+strlen(flags[j])+1;
                break;
            case 23:
                offsetstr = argv[1]+strlen(flags[j])+1;
                break;
            case 24:
                cliplowstr = argv[1]+strlen(flags[j])+1;
                break;
            case 25:
                cliphighstr = argv[1]+strlen(flags[j])+1;
                break;
            case 26:
                rad_smoothstr = argv[1]+strlen(flags[j])+1;

        }
        if(j < 3 || j==12)
        {
			if(NULL != cbf_cistrnstr(argv[1], ",flat",5))
                pack_flags |= CBF_FLAT_IMAGE;
            else
			if(NULL != cbf_cistrnstr(argv[1], ",uncorrelated",13))
                pack_flags |= CBF_UNCORRELATED_SECTIONS;
        }
        argc--;
        argv++;
    }

    while(argc > 1)
    {
        file_type = 0;
        strcpy(in_filename, argv[1]);
        i = strlen(in_filename);
        for(j = 0; endings[j] != NULL; j++)
        {
            k = strlen(endings[j]);
            if(endswith(in_filename, endings[j]))
            {
                file_type = j;
                break;
            }
        }
        if(NULL == endings[j])
        {
            fprintf(stderr,"adscimg2cbf: Input file name %s does not end in .img, .img.gz, or .img.bz2, or .img.Z\n", in_filename);
            exit(0);
        }
        strcpy(out_filename, in_filename);
        out_filename[i - k] = '\0';
        strcat(out_filename, ".cbf");

        if(0 == file_type)
        {
            if(NULL == (fp = fopen(in_filename, "rb")))
            {
                fprintf(stderr, "adscimg2cbf: Cannot open %s as input .img file\n", in_filename);
                exit(0);
            }
        }
        else
        {
            if(2 == file_type)
                sprintf(popen_command, "bzcat %s", in_filename);
            else
                sprintf(popen_command, "zcat %s", in_filename);
            if(NULL == (fp = popen(popen_command, "rb")))
            {
                fprintf(stderr, "adscimg2cbf: Cannot exec %s command to uncompress input file\n", popen_command);
                exit(0);
            }
        }

        /*
         *    Get the first header block.  Can't use seeks on input file.
         */

        if(NULL == (hptr = malloc(512 * sizeof (char))))
        {
            fprintf(stderr,"adscimg2cbf: cannot allocate memory for first 512 bytes of header of input file %s\n", in_filename);
            exit(0);
        }
        if(512 != (actread=fread(hptr, sizeof (char), 512, fp)))
        {
            fprintf(stderr, "adscimg2cbf: Cannot read first header block of file %s, actual read %d.\n", 
                          in_filename,actread);
            if(0 == file_type)
                fclose(fp);
            else
            {
                status_pclose = pclose(fp);
                if(0 != status_pclose)
                {
                    fprintf(stderr, "Status returned from uncompress command via popen NON-ZERO: %d\n", status_pclose);
                    perror("popen command (maybe this will be useful)");
                    fprintf(stderr, "Program exiting.  This may be evidence of a corrupt compressed file!\n");
                    fprintf(stderr, "Filename being uncompressed: %s with command: %s\n", in_filename, popen_command);
                    exit(0);
                }
            }
            exit(0);
        }
        for(i = 0; i < 5; i++)
            header_bytes[0 + i] = hptr[15 + i];

        header_bytes[5] = '\0';
        header_size_char = atoi(header_bytes);
        
        if(NULL == (hptr = realloc(hptr, header_size_char)))
        {
            fprintf(stderr,"adscimg2cbf: cannot reallocate memory for %d bytes of header of input file %s\n", 
                    header_size_char, in_filename);
            exit(0);
        }

        if(header_size_char > 512)
        {
            if((header_size_char - 512) != (actread=fread(hptr + 512, sizeof (char), 
                                                        (header_size_char - 512), fp)))
            {
                fprintf(stderr, "adscimg2cbf: Cannot read next %d bytes of header of file %s,"
                                                " actual read %d.\n", 
                        header_size_char - 512, in_filename, actread);
                if(0 == file_type)
                    fclose(fp);
                else
                {
                    status_pclose = pclose(fp);
                    if(0 != status_pclose)
                    {
                        fprintf(stderr, "Status returned from uncompress command via popen NON-ZERO: %d\n", status_pclose);
                        perror("popen command (maybe this will be useful)");
                        fprintf(stderr, "Program exiting.  This may be evidence of a corrupt compressed file!\n");
                        fprintf(stderr, "Filename being uncompressed: %s with command: %s\n", in_filename, popen_command);
                        exit(0);
                    }
                }
                exit(0);
            }
        }
        field[0] = '\0';
        gethd("SIZE1", field, hptr);
        if('\0' == field[0])
        {
            fprintf(stderr,"adscimg2cbf: keyword SIZE1 not found in header.  Cannot convert file %s\n", in_filename);
            exit(0);
        }
        size1 = atoi(field);
        
        field[0] = '\0';
        gethd("SIZE2", field, hptr);
        if('\0' == field[0])
        {
            fprintf(stderr,"adscimg2cbf: keyword SIZE2 not found in header.  Cannot convert file %s\n", in_filename);
            exit(0);
        }
        size2 = atoi(field);
        
        file_size = header_size_char + size1 * size2 * sizeof(unsigned short);

        if(NULL == (hptr = realloc(hptr, file_size)))
        {
            fprintf(stderr,"adscimg2cbf: cannot reallocate memory (size %d) for input file %s\n", 
                        file_size, in_filename);
            exit(0);
        }
        if((file_size - header_size_char) != (actread=fread(hptr + header_size_char, sizeof (char), 
                                                      (file_size - header_size_char), fp)))
        {
            fprintf(stderr, "adscimg2cbf: Cannot read data (size %d bytes) from input file %s."
                                        " actual read %d\n", 
                    file_size - header_size_char, in_filename, actread);
            if(0 == file_type)
                fclose(fp);
            else
            {
                status_pclose = pclose(fp);
                if(0 != status_pclose)
                {
                    fprintf(stderr, "Status returned from uncompress command via popen NON-ZERO: %d\n", status_pclose);
                    perror("popen command (maybe this will be useful)");
                    fprintf(stderr, "Program exiting.  This may be evidence of a corrupt compressed file!\n");
                    fprintf(stderr, "Filename being uncompressed: %s with command: %s\n", in_filename, popen_command);
                    exit(0);
                }
            }
            exit(0);
        }
        if(0 == file_type)
            fclose(fp);
        else
        {
            status_pclose = pclose(fp);
            if(0 != status_pclose)
            {
                fprintf(stderr, "Status returned from uncompress command via popen NON-ZERO: %d\n", status_pclose);
                perror("popen command (maybe this will be useful)");
                fprintf(stderr, "Program exiting.  This may be evidence of a corrupt compressed file!\n");
                fprintf(stderr, "Filename being uncompressed: %s with command: %s\n", in_filename, popen_command);
                exit(0);
            }
        }

        uptr = ((unsigned short *) (hptr + header_size_char));

        cbf_status = adscimg2cbf_sub2(hptr,
                                      uptr,
                                      out_filename,
                                      pack_flags,
                                      beam_center_convention,
                                      pad_flag,
                                      roi,
                                      thickstr,
                                      add_minicbf_header,
                                      polarstr,
                                      bin,
                                      scalestr,
                                      offsetstr,
                                      cliplowstr,
                                      cliphighstr,
                                      rad_smoothstr);
        free(hptr);

        argv++;
        argc--;
    }
    exit(0);
}
