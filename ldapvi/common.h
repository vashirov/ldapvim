/* Copyright (c) 2003,2004,2005,2006 David Lichteblau
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <ldap.h>
#include <ldap_schema.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/*
 * error.c
 */
#define syserr() do_syserr(__FILE__, __LINE__)

void do_syserr(char *file, int line);
void yourfault(char *str);
void ldaperr(LDAP *ld, char *str);

/*
 * arguments.c
 */
typedef struct cmdline {
	char *server;
	GPtrArray *basedns;
	int scope;
	char *filter;
	char **attrs;
	char *user;
	char *password;
	int progress;
	int referrals;
	GPtrArray *add;
	int managedsait;
	char *sortkeys;
	int starttls;
	int tls;
	int deref;
	int verbose;
	int noquestions;
	int discover;
	int config;
} cmdline;

void parse_arguments(
	int argc, const char **argv, cmdline *result, GPtrArray *ctrls);
void usage(int fd, int rc);

/*
 * data.c
 */
typedef struct named_array {
	char *name;
	GPtrArray *array;
} named_array;

typedef struct tentry {
	struct named_array e;
} tentry;
#define entry_dn(entry) ((entry)->e.name)
#define entry_attributes(entry) ((entry)->e.array)

typedef struct tattribute {
	struct named_array a;
} tattribute;
#define attribute_ad(attribute) ((attribute)->a.name)
#define attribute_values(attribute) ((attribute)->a.array)

tentry *entry_new(char *dn);
void entry_free(tentry *e);
int entry_cmp(tentry *e, tentry *f);

tattribute *attribute_new(char *ad);
void attribute_free(tattribute *a);
int attribute_cmp(tattribute *a, tattribute *b);

int named_array_ptr_cmp(const void *aa, const void *bb);

LDAPMod *attribute2mods(tattribute *attribute);
LDAPMod **entry2mods(tentry *entry);
tattribute *entry_find_attribute(tentry *entry, char *ad, int createp);
void attribute_append_value(tattribute *attribute, char *data, int n);
int attribute_find_value(tattribute *attribute, char *data, int n);
int attribute_remove_value(tattribute *a, char *data, int n);

struct berval *string2berval(GArray *s);
struct berval *gstring2berval(GString *s);
char *array2string(GArray *av);

/*
 * parse.c
 */
int peek_entry(FILE *s, long offset, char **key, long *pos);
int read_entry(FILE *s, long offset, char **key, tentry **entry, long *pos);
int read_rename(FILE *s, long offset, char **dn1, char **dn2, int *);
int read_modify(FILE *s, long offset, char **dn, LDAPMod ***mods);
int skip_entry(FILE *s, long offset, char **key);
int read_profile(FILE *s, tentry **entry);

/*
 * diff.c
 */
typedef int (*handler_change)(char *, char *, LDAPMod **, void *);
typedef int (*handler_rename)(char *, tentry *, void *);
typedef int (*handler_add)(char *, LDAPMod **, void *);
typedef int (*handler_delete)(char *, void *);
typedef int (*handler_rename0)(char *, char *, int, void *);

typedef struct thandler {
	handler_change change;
	handler_rename rename;
	handler_add add;
	handler_delete delete;
	handler_rename0 rename0;
} thandler;

LDAPMod **compare_entries(tentry *eclean, tentry *enew);
int compare_streams(
	thandler *handler,
	void *userdata,
	GArray *offsets,
	FILE *clean,
	FILE *data,
	long *error_position,
	long *syntax_error_position);

enum frob_rdn_mode {
	FROB_RDN_CHECK, FROB_RDN_REMOVE, FROB_RDN_ADD, FROB_RDN_CHECK_NONE
};
int frob_rdn(tentry *entry, char *dn, int mode);

/*
 * misc.c
 */
int carray_cmp(GArray *a, GArray *b);
int carray_ptr_cmp(const void *aa, const void *bb);
void cp(char *src, char *dst, off_t skip, int append);
char choose(char *prompt, char *charbag, char *help);
void edit_pos(char *pathname, long pos);
void edit(char *pathname, long line);
void view(char *pathname);
char *home_filename(char *name);
void read_ldapvi_history(void);
void write_ldapvi_history(void);
GString *getline(char *prompt);
char *get_password();
char *append(char *a, char *b);
void *xalloc(size_t size);
char *xdup(char *str);
void adjoin_str(GPtrArray *strs, char *str);

/*
 * print.c
 */
typedef enum t_print_binary_mode {
	PRINT_ASCII, PRINT_UTF8, PRINT_JUNK
} t_print_binary_mode;
extern t_print_binary_mode print_binary_mode;

void print_attrval(FILE *s, char *str, int len);
void print_entry_object(FILE *s, tentry *entry, char *key);
void print_ldapvi_modify(FILE *s, char *dn, LDAPMod **mods);
void print_ldapvi_rename(FILE *s, char *olddn, char *newdn, int deleteoldrdn);
void print_ldapvi_add(FILE *s, char *dn, LDAPMod **mods);
void print_ldapvi_delete(FILE *s, char *dn);
void print_ldif_modify(FILE *s, char *dn, LDAPMod **mods);
void print_ldif_rename(FILE *s, char *olddn, char *newdn, int deleteoldrdn);
void print_ldif_add(FILE *s, char *dn, LDAPMod **mods);
void print_ldif_delete(FILE *s, char *dn);

/*
 * search.c
 */
void discover_naming_contexts(LDAP *ld, GPtrArray *basedns);
void get_schema(LDAP *ld, GPtrArray *objectclasses, GPtrArray *attributes);
GArray *search(
	FILE *s, LDAP *ld, cmdline *cmdline, LDAPControl **ctrls, int notty);

/*
 * port.c
 */
typedef void (*on_exit_function)(int, void *);
int g_string_append_sha(GString *string, char *key);
int g_string_append_ssha(GString *string, char *key);
int g_string_append_md5(GString *string, char *key);
int g_string_append_smd5(GString *string, char *key);

/*
 * base64.c
 */
void print_base64(unsigned char const *src, size_t srclength, FILE *s);
void g_string_append_base64(
	GString *string, unsigned char const *src, size_t srclength);
int read_base64(char const *src, unsigned char *target, size_t targsize);
