#include <config.h>
#undef _ASSUAN_ONLY_GPG_ERRORS

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <logging.h>
#include <assuan.h>
#include <ksba.h>
#include <gcrypt.h>

#define FNAME "testcert.der"
static const char* fname;

assuan_context_t entry_ctx;

#define print_assuan_error( rc ) fprintf(stderr, "Error: %s", assuan_strerror( rc ) )

#define fail_if_err(a) do { if(a) {                                       \
                              fprintf (stderr, "%s:%d: KSBA error: %s\n", \
                              __FILE__, __LINE__, gpg_strerror(a));   \
                              exit (1); }                              \
                           } while(0)


#define fail_if_err2(f, a) do { if(a) {\
            fprintf (stderr, "%s:%d: KSBA error on file `%s': %s\n", \
                       __FILE__, __LINE__, (f), gpg_strerror(a));   \
                            exit (1); }                              \
                           } while(0)


static int
start_dirmngr (void)
{
  int rc;
  const char *pgmname;
  assuan_context_t ctx;
  const char *argv[3];

  if (entry_ctx)
    return 0; 

  log_debug ("no running DirMngr - starting it\n");

  if (fflush (NULL))
    {
      log_error ("error flushing pending output: %s\n", strerror (errno));
      return -1;
    }

  pgmname = "../src/dirmngr";

  argv[0] = pgmname;
  argv[1] = NULL;

  rc = assuan_pipe_connect (&ctx, pgmname, argv, 0);
  if (rc)
    {
      log_error ("can't connect to the DirMngr module: %s\n",
                 assuan_strerror (rc));
      return -1;
    }
  entry_ctx = ctx;

  log_debug ("connection to DirMngr established\n");

  if (0)
    {
      log_debug ("waiting for debugger [hit RETURN when ready] .....\n");
      getchar ();
      log_debug ("... okay\n");
    }

  return 0;
}

char *
get_certid (ksba_cert_t cert)
{
  ksba_sexp_t serial;
  unsigned char *p;
  char *endp;
  unsigned char hash[20];
  unsigned long n, i;
  char *certid;

  p = ksba_cert_get_issuer (cert, 0);
  if (!p)
    return NULL; /* Ooops: No issuer */
  gcry_md_hash_buffer (GCRY_MD_SHA1, hash, p, strlen (p));
  ksba_free (p);

  serial = ksba_cert_get_serial (cert);
  if (!serial)
    return NULL; /* oops: no serial number */
  p = serial;
  if (*p != '(')
    {
      log_error ("Ooops: invalid serial number\n");
      ksba_free (serial);
      return NULL;
    }
  p++;
  n = strtoul (p, &endp, 10);
  p = endp;
  if (*p != ':')
    {
      log_error ("Ooops: invalid serial number (no colon)\n");
      ksba_free (serial);
      return NULL;
    }
  p++;

  certid = malloc ( 40 + 1 + n*2 + 1);
  if (!certid)
    {
      return NULL; /* out of core */
    }

  for (i=0, endp = certid; i < 20; i++, endp += 2 )
    sprintf (endp, "%02X", hash[i]);
  *endp++ = '.';
  for (i=0; i < n; i++, endp += 2)
    sprintf (endp, "%02X", p[i]);
  *endp = 0;

  return certid;
}

static int inquire_done = 0;

static int 
sendcert( void* ctx, const char* line)
{
  FILE* infp;
  char buf[4096];
  int len = 0;
  fprintf(stderr, "######## Got inquiry \"%s\"\n", line );

/*   This hack is because we only want to send */
/*   the cert. Dirmngr will ask a second time  */
/*   -- this time for the issuer cert. */
/*    We dont know that, so we return nothing */
  if( inquire_done ) return ASSUAN_No_Error;
  inquire_done = 1;
  infp = fopen( fname, "r" );
  if( infp == NULL ) {
    perror("Error opening cert file");
    return ASSUAN_General_Error;
  }
  while( !feof( infp ) ) {
    len += fread( buf+len, 1, sizeof(buf)-len, infp );
  }
  assuan_send_data( ctx, buf, len);
  fclose( infp );
  return ASSUAN_No_Error;
}


int 
main( int argc, char** argv ) 
{
  int rc;
  char* line;
  char * certid;
  FILE* fp;
  ksba_reader_t r;
  ksba_cert_t cert;
  ksba_sexp_t sexp;
  int err;

  rc = start_dirmngr();
  if( rc )
    print_assuan_error(rc);
  if( argc > 1 )
    fname = argv[1];
  else
    fname = FNAME;

  fp = fopen ( fname, "r");
  if (!fp) {
    fprintf (stderr, "%s:%d: can't open `%s': %s\n",
	     __FILE__, __LINE__, fname, strerror (errno));
    exit (1);
  }

  err = ksba_reader_new (&r);
  if (err)
    fail_if_err (err);
  err = ksba_reader_set_file (r, fp);
  fail_if_err (err);

  err = ksba_cert_new (&cert);
  fail_if_err (err);

  err = ksba_cert_read_der (cert, r);
  fail_if_err2 (FNAME, err);

  printf ("Certificate in `%s':\n", fname);
  {
	int i = 0;
	ksba_name_t dp;
	ksba_name_t issuername;
				
	while( ksba_cert_get_crl_dist_point( cert, i, &dp, &issuername, NULL ) != -1 ) {
	  char* dps;
	  char* issuernames;
	  
	  dps = ksba_name_get_uri( dp, 0 );
	  issuernames = ksba_name_get_uri( issuername, 0 );
	  fprintf( stderr, "dps=%s, issuernames=%s\n", dps, issuernames ); 
	  if( !dps || !issuernames ) break;
	}
  }
  sexp = ksba_cert_get_serial (cert);
  //fputs ("  serial....: ", stdout);
  //print_sexp (sexp);
  ksba_free (sexp);
  putchar ('\n');

  certid = get_certid( cert );
  if( !certid ) {
    fprintf(stderr, "No certid!\n" );
    exit(-1);
  }
  line = malloc( strlen( certid ) + sizeof("ISVALID "));
  strcpy( line, "ISVALID " );
  strcat( line, certid );

  fprintf(stderr, "sending \"%s\"\n", line );

  rc = assuan_transact (entry_ctx, line, NULL, NULL, sendcert, entry_ctx, NULL, NULL);
  if( rc ) {
    fprintf(stderr,"\n#######");
    print_assuan_error(rc);
  } else fprintf(stderr,"\n#######Cert OK\n");
  fprintf(stderr,"\nDone\n");
  return rc;
}
