TESTS_ENVIRONMENT = 

AM_CFLAGS = -I$(top_srcdir)/jnlib $(LIBASSUAN_CFLAGS) \
	      $(LIBGCRYPT_CFLAGS) $(KSBA_CFLAGS)
AM_LDFLAGS = $(LDFLAGS)

# test_certs = cert_dfn_pca01.der 

# EXTRA_DIST = $(test_certs)

TESTS = 

noinst_PROGRAMS = test-dirmngr asschk show-multi percent-escape
LDADD = ../jnlib/libjnlib.a $(LIBASSUAN_LIBS) $(LIBGCRYPT_LIBS) $(KSBA_LIBS) \
       $(LIBINTL) $(LIBICONV)

asschk_SOURCES = asschk.c

show_multi_SOURCERS = show-multi.c
show_multi_LDADD = 

percent_escape_SOURCERS = percent-escape.c
percent_escape_LDADD = 

