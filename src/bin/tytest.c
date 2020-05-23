#include "private.h"
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "private.h"
#include <Elementary.h>
#include "config.h"
#include "termpty.h"
#include "termptyops.h"
#include "termiointernals.h"
#include "tytest.h"
#include "unit_tests.h"
#include "tytest_common.h"

#include "md5/md5.h"

int _log_domain = -1;

/* {{{ Unit tests */

static struct {
     const char *name;
     tytest_func func;
} _tytests[] = {
       { "dummy", tytest_dummy },
       { "sb", tytest_sb},
       { NULL, NULL},
};

static int
_run_this_tytest(const char *name, tytest_func func)
{
   int res;
   fprintf(stderr, "\033[0m%s...", name);
   res = func();
   fprintf(stderr, " %s\n", res == 0 ? "\033[32m✔" : "\033[31;1m×");
   return res;
}

static tytest_func
_find_tytest(const char *name)
{
   int ntests = (sizeof(_tytests) / sizeof(_tytests[0])) - 1;
   int i;

   for (i = 0; i < ntests; i++)
     {
        if (strcmp(name, _tytests[i].name) == 0)
          return _tytests[i].func;
     }
   return NULL;
}

static int
_run_all_tytests(void)
{
   int ntests = (sizeof(_tytests) / sizeof(_tytests[0])) - 1;
   int i, res = 0;

   for (i = 0; res == 0 && i < ntests; i++)
     res = _run_this_tytest(_tytests[i].name, _tytests[i].func);
   return res;
}

static int
_run_tytests(int argc, char **argv)
{
   int i, res = 0;

   for (i = 1; res == 0 && i < argc; i++)
     {
        if (strncmp(argv[i], "all", strlen("all")) == 0)
          res = _run_all_tytests();
        else
          {
             tytest_func func = _find_tytest(argv[i]);
             if (!func)
               {
                  fprintf(stderr, "can not find test named '%s'\n", argv[i]);
                  return -1;
               }
             res = _run_this_tytest(argv[i], func);
          }
     }
   return res;
}

/* }}} */

typedef struct _Termpty_Tests
{
   size_t backsize, backpos;
   Backlog_Beacon backlog_beacon;
   Term_State termstate;
   Term_Cursor cursor_state;
   Term_Cursor cursor_save[2];
   int w, h;
   unsigned int altbuf     : 1;
   unsigned int mouse_mode : 3;
   unsigned int mouse_ext  : 2;
   unsigned int bracketed_paste : 1;
} Termpty_Tests;

static void
_termpty_to_termpty_tests(Termpty *ty, Termpty_Tests *tt)
{
   memset(tt, '\0', sizeof(*tt));
   tt->backsize = ty->backsize;
   tt->backpos = ty->backpos;
   tt->backlog_beacon = ty->backlog_beacon;
   tt->termstate = ty->termstate;
   tt->cursor_state = ty->cursor_state;
   tt->cursor_save[0] = ty->cursor_save[0];
   tt->cursor_save[1] = ty->cursor_save[1];
   tt->w = ty->w;
   tt->h = ty->h;
   tt->altbuf = ty->altbuf;
   tt->mouse_mode = ty->mouse_mode;
   tt->mouse_ext = ty->mouse_ext;
   tt->bracketed_paste = ty->bracketed_paste;
}

static void
_tytest_checksum(Termpty *ty)
{
   MD5_CTX ctx;
   Termpty_Tests tests;
   char md5out[(2 * MD5_HASHBYTES) + 1];
   unsigned char hash[MD5_HASHBYTES];
   static const char hex[] = "0123456789abcdef";
   int n;

   _termpty_to_termpty_tests(ty, &tests);

   MD5Init(&ctx);
   /* Termpty */
   MD5Update(&ctx,
             (unsigned char const*)&tests,
             sizeof(tests));
   /* The screens */
   MD5Update(&ctx,
             (unsigned char const*)ty->screen,
             sizeof(Termcell) * ty->w * ty->h);
   MD5Update(&ctx,
             (unsigned char const*)ty->screen2,
             sizeof(Termcell) * ty->w * ty->h);
   /* Icon/Title */
   if (ty->prop.icon)
     {
        MD5Update(&ctx,
                  (unsigned char const*)ty->prop.icon,
                  strlen(ty->prop.icon));
     }
   else
     {
        MD5Update(&ctx, (unsigned char const*)"(NULL)", 6);
     }
   if (ty->prop.title)
     {
        MD5Update(&ctx,
                  (unsigned char const*)ty->prop.title,
                  strlen(ty->prop.title));
     }
   else
     {
        MD5Update(&ctx, (unsigned char const*)"(NULL)", 6);
     }
   /* Cursor shape */
   const char *cursor_shape = tytest_cursor_shape_get();
   MD5Update(&ctx, (unsigned char const*)cursor_shape,
             strlen(cursor_shape));
   /* Write buffer */
   if (ty->write_buffer.buf)
     {
        MD5Update(&ctx, (unsigned char const*)ty->write_buffer.buf,
                  ty->write_buffer.len);
     }

   MD5Final(hash, &ctx);

   for (n = 0; n < MD5_HASHBYTES; n++)
     {
        md5out[2 * n] = hex[hash[n] >> 4];
        md5out[2 * n + 1] = hex[hash[n] & 0x0f];
     }
   md5out[2 * MD5_HASHBYTES] = '\0';
   printf("%s", md5out);
}


int
main(int argc, char **argv)
{
   if (argc > 1)
     return _run_tytests(argc, argv);

   eina_init();

   _log_domain = eina_log_domain_register("tytest", NULL);

   tytest_common_init();

   tytest_common_main_loop();

   _tytest_checksum(tytest_termpty_get());

   tytest_common_shutdown();

   eina_shutdown();

   return 0;
}
