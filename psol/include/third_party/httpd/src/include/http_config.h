/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file http_config.h
 * @brief Apache Configuration
 *
 * @defgroup APACHE_CORE_CONFIG Configuration
 * @ingroup  APACHE_CORE
 * @{
 */

#ifndef APACHE_HTTP_CONFIG_H
#define APACHE_HTTP_CONFIG_H

#include "apr_hooks.h"
#include "util_cfgtree.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The central data structures around here...
 */

/* Command dispatch structures... */

/**
 * How the directives arguments should be parsed.
 * @remark Note that for all of these except RAW_ARGS, the config routine is
 *      passed a freshly allocated string which can be modified or stored
 *      or whatever...
 */
enum cmd_how {
    RAW_ARGS,			/**< cmd_func parses command line itself */
    TAKE1,			/**< one argument only */
    TAKE2,			/**< two arguments only */
    ITERATE,			/**< one argument, occuring multiple times
				 * (e.g., IndexIgnore)
				 */
    ITERATE2,			/**< two arguments, 2nd occurs multiple times
				 * (e.g., AddIcon)
				 */
    FLAG,			/**< One of 'On' or 'Off' */
    NO_ARGS,			/**< No args at all, e.g. </Directory> */
    TAKE12,			/**< one or two arguments */
    TAKE3,			/**< three arguments only */
    TAKE23,			/**< two or three arguments */
    TAKE123,			/**< one, two or three arguments */
    TAKE13,			/**< one or three arguments */
    TAKE_ARGV			/**< an argc and argv are passed */
};
/**
 * This structure is passed to a command which is being invoked,
 * to carry a large variety of miscellaneous data which is all of
 * use to *somebody*...
 */
typedef struct cmd_parms_struct cmd_parms;

#if defined(AP_HAVE_DESIGNATED_INITIALIZER) || defined(DOXYGEN)

/** 
 * All the types of functions that can be used in directives
 * @internal
 */
typedef union {
    /** function to call for a no-args */
    const char *(*no_args) (cmd_parms *parms, void *mconfig);
    /** function to call for a raw-args */
    const char *(*raw_args) (cmd_parms *parms, void *mconfig,
			     const char *args);
    /** function to call for a argv/argc */
    const char *(*take_argv) (cmd_parms *parms, void *mconfig,
			     int argc, char *const argv[]);
    /** function to call for a take1 */
    const char *(*take1) (cmd_parms *parms, void *mconfig, const char *w);
    /** function to call for a take2 */
    const char *(*take2) (cmd_parms *parms, void *mconfig, const char *w,
			  const char *w2);
    /** function to call for a take3 */
    const char *(*take3) (cmd_parms *parms, void *mconfig, const char *w,
			  const char *w2, const char *w3);
    /** function to call for a flag */
    const char *(*flag) (cmd_parms *parms, void *mconfig, int on);
} cmd_func;

/** This configuration directive does not take any arguments */
# define AP_NO_ARGS	func.no_args
/** This configuration directive will handle it's own parsing of arguments*/
# define AP_RAW_ARGS	func.raw_args
/** This configuration directive will handle it's own parsing of arguments*/
# define AP_TAKE_ARGV	func.take_argv
/** This configuration directive takes 1 argument*/
# define AP_TAKE1	func.take1
/** This configuration directive takes 2 arguments */
# define AP_TAKE2	func.take2
/** This configuration directive takes 3 arguments */
# define AP_TAKE3	func.take3
/** This configuration directive takes a flag (on/off) as a argument*/
# define AP_FLAG	func.flag

/** method of declaring a directive with no arguments */
# define AP_INIT_NO_ARGS(directive, func, mconfig, where, help) \
    { directive, { .no_args=func }, mconfig, where, RAW_ARGS, help }
/** method of declaring a directive with raw argument parsing */
# define AP_INIT_RAW_ARGS(directive, func, mconfig, where, help) \
    { directive, { .raw_args=func }, mconfig, where, RAW_ARGS, help }
/** method of declaring a directive with raw argument parsing */
# define AP_INIT_TAKE_ARGV(directive, func, mconfig, where, help) \
    { directive, { .take_argv=func }, mconfig, where, TAKE_ARGV, help }
/** method of declaring a directive which takes 1 argument */
# define AP_INIT_TAKE1(directive, func, mconfig, where, help) \
    { directive, { .take1=func }, mconfig, where, TAKE1, help }
/** method of declaring a directive which takes multiple arguments */
# define AP_INIT_ITERATE(directive, func, mconfig, where, help) \
    { directive, { .take1=func }, mconfig, where, ITERATE, help }
/** method of declaring a directive which takes 2 arguments */
# define AP_INIT_TAKE2(directive, func, mconfig, where, help) \
    { directive, { .take2=func }, mconfig, where, TAKE2, help }
/** method of declaring a directive which takes 1 or 2 arguments */
# define AP_INIT_TAKE12(directive, func, mconfig, where, help) \
    { directive, { .take2=func }, mconfig, where, TAKE12, help }
/** method of declaring a directive which takes multiple 2 arguments */
# define AP_INIT_ITERATE2(directive, func, mconfig, where, help) \
    { directive, { .take2=func }, mconfig, where, ITERATE2, help }
/** method of declaring a directive which takes 1 or 3 arguments */
# define AP_INIT_TAKE13(directive, func, mconfig, where, help) \
    { directive, { .take3=func }, mconfig, where, TAKE13, help }
/** method of declaring a directive which takes 2 or 3 arguments */
# define AP_INIT_TAKE23(directive, func, mconfig, where, help) \
    { directive, { .take3=func }, mconfig, where, TAKE23, help }
/** method of declaring a directive which takes 1 to 3 arguments */
# define AP_INIT_TAKE123(directive, func, mconfig, where, help) \
    { directive, { .take3=func }, mconfig, where, TAKE123, help }
/** method of declaring a directive which takes 3 arguments */
# define AP_INIT_TAKE3(directive, func, mconfig, where, help) \
    { directive, { .take3=func }, mconfig, where, TAKE3, help }
/** method of declaring a directive which takes a flag (on/off) as a argument*/
# define AP_INIT_FLAG(directive, func, mconfig, where, help) \
    { directive, { .flag=func }, mconfig, where, FLAG, help }

#else /* AP_HAVE_DESIGNATED_INITIALIZER */

typedef const char *(*cmd_func) ();

# define AP_NO_ARGS  func
# define AP_RAW_ARGS func
# define AP_TAKE_ARGV func
# define AP_TAKE1    func
# define AP_TAKE2    func
# define AP_TAKE3    func
# define AP_FLAG     func

# define AP_INIT_NO_ARGS(directive, func, mconfig, where, help) \
    { directive, func, mconfig, where, RAW_ARGS, help }
# define AP_INIT_RAW_ARGS(directive, func, mconfig, where, help) \
    { directive, func, mconfig, where, RAW_ARGS, help }
# define AP_INIT_TAKE_ARGV(directive, func, mconfig, where, help) \
    { directive, func, mconfig, where, TAKE_ARGV, help }
# define AP_INIT_TAKE1(directive, func, mconfig, where, help) \
    { directive, func, mconfig, where, TAKE1, help }
# define AP_INIT_ITERATE(directive, func, mconfig, where, help) \
    { directive, func, mconfig, where, ITERATE, help }
# define AP_INIT_TAKE2(directive, func, mconfig, where, help) \
    { directive, func, mconfig, where, TAKE2, help }
# define AP_INIT_TAKE12(directive, func, mconfig, where, help) \
    { directive, func, mconfig, where, TAKE12, help }
# define AP_INIT_ITERATE2(directive, func, mconfig, where, help) \
    { directive, func, mconfig, where, ITERATE2, help }
# define AP_INIT_TAKE13(directive, func, mconfig, where, help) \
    { directive, func, mconfig, where, TAKE13, help }
# define AP_INIT_TAKE23(directive, func, mconfig, where, help) \
    { directive, func, mconfig, where, TAKE23, help }
# define AP_INIT_TAKE123(directive, func, mconfig, where, help) \
    { directive, func, mconfig, where, TAKE123, help }
# define AP_INIT_TAKE3(directive, func, mconfig, where, help) \
    { directive, func, mconfig, where, TAKE3, help }
# define AP_INIT_FLAG(directive, func, mconfig, where, help) \
    { directive, func, mconfig, where, FLAG, help }

#endif /* AP_HAVE_DESIGNATED_INITIALIZER */

/**
 * The command record structure.  Each modules can define a table of these
 * to define the directives it will implement.
 */
typedef struct command_struct command_rec; 
struct command_struct {
    /** Name of this command */
    const char *name;
    /** The function to be called when this directive is parsed */
    cmd_func func;
    /** Extra data, for functions which implement multiple commands... */
    void *cmd_data;		
    /** What overrides need to be allowed to enable this command. */
    int req_override;
    /** What the command expects as arguments 
     *  @defvar cmd_how args_how*/
    enum cmd_how args_how;

    /** 'usage' message, in case of syntax errors */
    const char *errmsg;
};

/**
 * @defgroup ConfigDirectives Allowed locations for configuration directives.
 *
 * The allowed locations for a configuration directive are the union of
 * those indicated by each set bit in the req_override mask.
 *
 * @{
 */
#define OR_NONE 0             /**< *.conf is not available anywhere in this override */
#define OR_LIMIT 1	     /**< *.conf inside <Directory> or <Location>
				and .htaccess when AllowOverride Limit */
#define OR_OPTIONS 2         /**< *.conf anywhere
                                and .htaccess when AllowOverride Options */
#define OR_FILEINFO 4        /**< *.conf anywhere
				and .htaccess when AllowOverride FileInfo */
#define OR_AUTHCFG 8         /**< *.conf inside <Directory> or <Location>
				and .htaccess when AllowOverride AuthConfig */
#define OR_INDEXES 16        /**< *.conf anywhere
				and .htaccess when AllowOverride Indexes */
#define OR_UNSET 32          /**< unset a directive (in Allow) */
#define ACCESS_CONF 64       /**< *.conf inside <Directory> or <Location> */
#define RSRC_CONF 128	     /**< *.conf outside <Directory> or <Location> */
#define EXEC_ON_READ 256     /**< force directive to execute a command 
                which would modify the configuration (like including another
                file, or IFModule */
/** this directive can be placed anywhere */
#define OR_ALL (OR_LIMIT|OR_OPTIONS|OR_FILEINFO|OR_AUTHCFG|OR_INDEXES)

/** @} */

/**
 * This can be returned by a function if they don't wish to handle
 * a command. Make it something not likely someone will actually use
 * as an error code.
 */
#define DECLINE_CMD "\a\b"

/** Common structure for reading of config files / passwd files etc. */
typedef struct ap_configfile_t ap_configfile_t;
struct ap_configfile_t {
    int (*getch) (void *param);	    /**< a getc()-like function */
    void *(*getstr) (void *buf, size_t bufsiz, void *param);
				    /**< a fgets()-like function */
    int (*close) (void *param);	    /**< a close handler function */
    void *param;                    /**< the argument passed to getch/getstr/close */
    const char *name;               /**< the filename / description */
    unsigned line_number;           /**< current line number, starting at 1 */
};

/**
 * This structure is passed to a command which is being invoked,
 * to carry a large variety of miscellaneous data which is all of
 * use to *somebody*...
 */
struct cmd_parms_struct {
    /** Argument to command from cmd_table */
    void *info;
    /** Which allow-override bits are set */
    int override;
    /** Which methods are <Limit>ed */
    apr_int64_t limited;
    /** methods which are limited */
    apr_array_header_t *limited_xmethods;
    /** methods which are xlimited */
    ap_method_list_t *xlimited;

    /** Config file structure. */
    ap_configfile_t *config_file;
    /** the directive specifying this command */
    ap_directive_t *directive;

    /** Pool to allocate new storage in */
    apr_pool_t *pool;
    /** Pool for scratch memory; persists during configuration, but 
     *  wiped before the first request is served...  */
    apr_pool_t *temp_pool;
    /** Server_rec being configured for */
    server_rec *server;
    /** If configuring for a directory, pathname of that directory.  
     *  NOPE!  That's what it meant previous to the existance of <Files>, 
     * <Location> and regex matching.  Now the only usefulness that can be 
     * derived from this field is whether a command is being called in a 
     * server context (path == NULL) or being called in a dir context 
     * (path != NULL).  */
    char *path;
    /** configuration command */
    const command_rec *cmd;

    /** per_dir_config vector passed to handle_command */
    struct ap_conf_vector_t *context;
    /** directive with syntax error */
    const ap_directive_t *err_directive;

    /** Which allow-override-opts bits are set */
    int override_opts;
};

/**
 * Module structures.  Just about everything is dispatched through
 * these, directly or indirectly (through the command and handler
 * tables).
 */
typedef struct module_struct module;
struct module_struct {
    /** API version, *not* module version; check that module is 
     * compatible with this version of the server.
     */
    int version;
    /** API minor version. Provides API feature milestones. Not checked 
     *  during module init */
    int minor_version;
    /** Index to this modules structures in config vectors.  */
    int module_index;

    /** The name of the module's C file */
    const char *name;
    /** The handle for the DSO.  Internal use only */
    void *dynamic_load_handle;

    /** A pointer to the next module in the list
     *  @defvar module_struct *next */
    struct module_struct *next;

    /** Magic Cookie to identify a module structure;  It's mainly 
     *  important for the DSO facility (see also mod_so).  */
    unsigned long magic;

    /** Function to allow MPMs to re-write command line arguments.  This
     *  hook is only available to MPMs.
     *  @param The process that the server is running in.
     */
    void (*rewrite_args) (process_rec *process);
    /** Function to allow all modules to create per directory configuration
     *  structures.
     *  @param p The pool to use for all allocations.
     *  @param dir The directory currently being processed.
     *  @return The per-directory structure created
     */
    void *(*create_dir_config) (apr_pool_t *p, char *dir);
    /** Function to allow all modules to merge the per directory configuration
     *  structures for two directories.
     *  @param p The pool to use for all allocations.
     *  @param base_conf The directory structure created for the parent directory.
     *  @param new_conf The directory structure currently being processed.
     *  @return The new per-directory structure created
     */
    void *(*merge_dir_config) (apr_pool_t *p, void *base_conf, void *new_conf);
    /** Function to allow all modules to create per server configuration
     *  structures.
     *  @param p The pool to use for all allocations.
     *  @param s The server currently being processed.
     *  @return The per-server structure created
     */
    void *(*create_server_config) (apr_pool_t *p, server_rec *s);
    /** Function to allow all modules to merge the per server configuration
     *  structures for two servers.
     *  @param p The pool to use for all allocations.
     *  @param base_conf The directory structure created for the parent directory.
     *  @param new_conf The directory structure currently being processed.
     *  @return The new per-directory structure created
     */
    void *(*merge_server_config) (apr_pool_t *p, void *base_conf, 
                                  void *new_conf);

    /** A command_rec table that describes all of the directives this module
     * defines. */
    const command_rec *cmds;

    /** A hook to allow modules to hook other points in the request processing.
     *  In this function, modules should call the ap_hook_*() functions to
     *  register an interest in a specific step in processing the current
     *  request.
     *  @param p the pool to use for all allocations
     */
    void (*register_hooks) (apr_pool_t *p);
};

/**
 * @defgroup ModuleInit Module structure initializers
 *
 * Initializer for the first few module slots, which are only
 * really set up once we start running.  Note that the first two slots
 * provide a version check; this should allow us to deal with changes to
 * the API. The major number should reflect changes to the API handler table
 * itself or removal of functionality. The minor number should reflect
 * additions of functionality to the existing API. (the server can detect
 * an old-format module, and either handle it back-compatibly, or at least
 * signal an error). See src/include/ap_mmn.h for MMN version history.
 * @{
 */

/** The one used in Apache 1.3, which will deliberately cause an error */
#define STANDARD_MODULE_STUFF	this_module_needs_to_be_ported_to_apache_2_0

/** Use this in all standard modules */
#define STANDARD20_MODULE_STUFF	MODULE_MAGIC_NUMBER_MAJOR, \
				MODULE_MAGIC_NUMBER_MINOR, \
				-1, \
				__FILE__, \
				NULL, \
				NULL, \
				MODULE_MAGIC_COOKIE, \
                                NULL      /* rewrite args spot */

/** Use this only in MPMs */
#define MPM20_MODULE_STUFF	MODULE_MAGIC_NUMBER_MAJOR, \
				MODULE_MAGIC_NUMBER_MINOR, \
				-1, \
				__FILE__, \
				NULL, \
				NULL, \
				MODULE_MAGIC_COOKIE

/** @} */

/* CONFIGURATION VECTOR FUNCTIONS */

/** configuration vector structure */
typedef struct ap_conf_vector_t ap_conf_vector_t;

/**
 * Generic accessors for other modules to get at their own module-specific
 * data
 * @param conf_vector The vector in which the modules configuration is stored.
 *        usually r->per_dir_config or s->module_config
 * @param m The module to get the data for.
 * @return The module-specific data
 */
AP_DECLARE(void *) ap_get_module_config(const ap_conf_vector_t *cv,
                                        const module *m);

/**
 * Generic accessors for other modules to set at their own module-specific
 * data
 * @param conf_vector The vector in which the modules configuration is stored.
 *        usually r->per_dir_config or s->module_config
 * @param m The module to set the data for.
 * @param val The module-specific data to set
 */
AP_DECLARE(void) ap_set_module_config(ap_conf_vector_t *cv, const module *m,
                                      void *val);

#if !defined(AP_DEBUG)

#define ap_get_module_config(v,m)	\
    (((void **)(v))[(m)->module_index])
#define ap_set_module_config(v,m,val)	\
    ((((void **)(v))[(m)->module_index]) = (val))

#endif /* AP_DEBUG */


/**
 * Generic command handling function for strings
 * @param cmd The command parameters for this directive
 * @param struct_ptr pointer into a given type
 * @param arg The argument to the directive
 * @return An error string or NULL on success
 */
AP_DECLARE_NONSTD(const char *) ap_set_string_slot(cmd_parms *cmd, 
                                                   void *struct_ptr,
                                                   const char *arg);

/**
 * Generic command handling function for integers
 * @param cmd The command parameters for this directive
 * @param struct_ptr pointer into a given type
 * @param arg The argument to the directive
 * @return An error string or NULL on success
 */
AP_DECLARE_NONSTD(const char *) ap_set_int_slot(cmd_parms *cmd, 
                                                void *struct_ptr,
                                                const char *arg);

/**
 * Return true if the specified method is limited by being listed in
 * a <Limit> container, or by *not* being listed in a <LimiteExcept>
 * container.
 *
 * @param   method  Pointer to a string specifying the method to check.
 * @param   cmd     Pointer to the cmd_parms structure passed to the
 *                  directive handler.
 * @return  0 if the method is not limited in the current scope
 */
AP_DECLARE(int) ap_method_is_limited(cmd_parms *cmd, const char *method);

/**
 * Generic command handling function for strings, always sets the value
 * to a lowercase string
 * @param cmd The command parameters for this directive
 * @param struct_ptr pointer into a given type
 * @param arg The argument to the directive
 * @return An error string or NULL on success
 */
AP_DECLARE_NONSTD(const char *) ap_set_string_slot_lower(cmd_parms *cmd, 
                                                         void *struct_ptr, 
                                                         const char *arg);
/**
 * Generic command handling function for flags
 * @param cmd The command parameters for this directive
 * @param struct_ptr pointer into a given type
 * @param arg The argument to the directive (either 1 or 0)
 * @return An error string or NULL on success
 */
AP_DECLARE_NONSTD(const char *) ap_set_flag_slot(cmd_parms *cmd, 
                                                 void *struct_ptr, 
                                                 int arg);
/**
 * Generic command handling function for files
 * @param cmd The command parameters for this directive
 * @param struct_ptr pointer into a given type
 * @param arg The argument to the directive
 * @return An error string or NULL on success
 */
AP_DECLARE_NONSTD(const char *) ap_set_file_slot(cmd_parms *cmd, 
                                                 void *struct_ptr, 
                                                 const char *arg);
/**
 * Generic command handling function to respond with cmd->help as an error
 * @param cmd The command parameters for this directive
 * @param struct_ptr pointer into a given type
 * @param arg The argument to the directive
 * @return The cmd->help value as the error string
 * @tip This allows simple declarations such as;
 * <pre>
 *     AP_INIT_RAW_ARGS("Foo", ap_set_deprecated, NULL, OR_ALL, 
 *         "The Foo directive is no longer supported, use Bar"),
 * </pre>
 */
AP_DECLARE_NONSTD(const char *) ap_set_deprecated(cmd_parms *cmd, 
                                                  void *struct_ptr, 
                                                  const char *arg);
/**
 * For modules which need to read config files, open logs, etc. this returns
 * the canonical form of fname made absolute to ap_server_root.
 * @param p pool to allocate data from
 * @param fname The file name
 */
AP_DECLARE(char *) ap_server_root_relative(apr_pool_t *p, const char *fname);

/* Finally, the hook for dynamically loading modules in... */

/**
 * Add a module to the server
 * @param m The module structure of the module to add
 * @param p The pool of the same lifetime as the module
 */
AP_DECLARE(const char *) ap_add_module(module *m, apr_pool_t *p);

/**
 * Remove a module from the server.  There are some caveats:
 * when the module is removed, its slot is lost so all the current
 * per-dir and per-server configurations are invalid. So we should
 * only ever call this function when you are invalidating almost
 * all our current data. I.e. when doing a restart.
 * @param m the module structure of the module to remove
 */
AP_DECLARE(void) ap_remove_module(module *m);
/**
 * Add a module to the chained modules list and the list of loaded modules
 * @param m The module structure of the module to add
 * @param p The pool with the same lifetime as the module
 */
AP_DECLARE(const char *) ap_add_loaded_module(module *mod, apr_pool_t *p);
/**
 * Remove a module fromthe chained modules list and the list of loaded modules
 * @param m the module structure of the module to remove
 */
AP_DECLARE(void) ap_remove_loaded_module(module *mod);
/**
 * Find the name of the specified module
 * @param m The module to get the name for
 * @return the name of the module
 */
AP_DECLARE(const char *) ap_find_module_name(module *m);
/**
 * Find a module based on the name of the module
 * @param name the name of the module
 * @return the module structure if found, NULL otherwise
 */
AP_DECLARE(module *) ap_find_linked_module(const char *name);

/**
 * Open a ap_configfile_t as apr_file_t
 * @param ret_cfg open ap_configfile_t struct pointer
 * @param p The pool to allocate the structure from
 * @param name the name of the file to open
 */
AP_DECLARE(apr_status_t) ap_pcfg_openfile(ap_configfile_t **ret_cfg, 
                                          apr_pool_t *p, const char *name);

/**
 * Allocate a ap_configfile_t handle with user defined functions and params 
 * @param p The pool to allocate from
 * @param descr The name of the file
 * @param param The argument passed to getch/getstr/close
 * @param getc_func The getch function
 * @param gets_func The getstr function
 * @param close_func The close function
 */
AP_DECLARE(ap_configfile_t *) ap_pcfg_open_custom(apr_pool_t *p, 
    const char *descr,
    void *param,
    int(*getc_func)(void*),
    void *(*gets_func) (void *buf, size_t bufsiz, void *param),
    int(*close_func)(void *param));

/**
 * Read one line from open ap_configfile_t, strip LF, increase line number
 * @param buf place to store the line read
 * @param bufsize size of the buffer
 * @param cfp File to read from
 * @return 1 on success, 0 on failure
 */
AP_DECLARE(int) ap_cfg_getline(char *buf, size_t bufsize, ap_configfile_t *cfp);

/**
 * Read one char from open configfile_t, increase line number upon LF 
 * @param cfp The file to read from
 * @return the character read
 */
AP_DECLARE(int) ap_cfg_getc(ap_configfile_t *cfp);

/**
 * Detach from open ap_configfile_t, calling the close handler
 * @param cfp The file to close
 * @return 1 on sucess, 0 on failure
 */
AP_DECLARE(int) ap_cfg_closefile(ap_configfile_t *cfp);

/**
 * Read all data between the current <foo> and the matching </foo>.  All
 * of this data is forgotten immediately.  
 * @param cmd The cmd_parms to pass to the directives inside the container
 * @param directive The directive name to read until
 * @return Error string on failure, NULL on success
 */
AP_DECLARE(const char *) ap_soak_end_container(cmd_parms *cmd, char *directive);

/**
 * Read all data between the current <foo> and the matching </foo> and build
 * a config tree from it
 * @param p pool to allocate from
 * @param temp_pool Temporary pool to allocate from
 * @param parms The cmd_parms to pass to all directives read
 * @param current The current node in the tree
 * @param curr_parent The current parent node
 * @param orig_directive The directive to read until hit.
 * @return Error string on failure, NULL on success
*/
AP_DECLARE(const char *) ap_build_cont_config(apr_pool_t *p, 
                                              apr_pool_t *temp_pool,
                                              cmd_parms *parms,
                                              ap_directive_t **current,
                                              ap_directive_t **curr_parent,
                                              char *orig_directive);

/**
 * Build a config tree from a config file
 * @param parms The cmd_parms to pass to all of the directives in the file
 * @param conf_pool The pconf pool
 * @param temp_pool The temporary pool
 * @param conftree Place to store the root node of the config tree
 * @return Error string on erro, NULL otherwise
 */
AP_DECLARE(const char *) ap_build_config(cmd_parms *parms,
                                         apr_pool_t *conf_pool,
                                         apr_pool_t *temp_pool,
                                         ap_directive_t **conftree);

/**
 * Walk a config tree and setup the server's internal structures
 * @param conftree The config tree to walk
 * @param parms The cmd_parms to pass to all functions
 * @param section_vector The per-section config vector.
 * @return Error string on error, NULL otherwise
 */
AP_DECLARE(const char *) ap_walk_config(ap_directive_t *conftree,
                                        cmd_parms *parms,
                                        ap_conf_vector_t *section_vector);

/**
 * @defgroup ap_check_cmd_context Check command context
 * @{
 */
/**
 * Check the context a command is used in.
 * @param cmd The command to check
 * @param forbidden Where the command is forbidden.
 * @return Error string on error, NULL on success
 */
AP_DECLARE(const char *) ap_check_cmd_context(cmd_parms *cmd, 
                                              unsigned forbidden);

#define  NOT_IN_VIRTUALHOST     0x01 /**< Forbidden in <Virtualhost> */
#define  NOT_IN_LIMIT           0x02 /**< Forbidden in <Limit> */
#define  NOT_IN_DIRECTORY       0x04 /**< Forbidden in <Directory> */
#define  NOT_IN_LOCATION        0x08 /**< Forbidden in <Location> */
#define  NOT_IN_FILES           0x10 /**< Forbidden in <Files> */
/** Forbidden in <Directory>/<Location>/<Files>*/
#define  NOT_IN_DIR_LOC_FILE    (NOT_IN_DIRECTORY|NOT_IN_LOCATION|NOT_IN_FILES) 
/** Forbidden in <VirtualHost>/<Limit>/<Directory>/<Location>/<Files> */
#define  GLOBAL_ONLY            (NOT_IN_VIRTUALHOST|NOT_IN_LIMIT|NOT_IN_DIR_LOC_FILE) 

/** @} */

#ifdef CORE_PRIVATE

/**
 * @brief This structure is used to assign symbol names to module pointers
 */
typedef struct {
    const char *name;
    module *modp;
} ap_module_symbol_t;

/**
 * The topmost module in the list
 * @defvar module *ap_top_module
 */
AP_DECLARE_DATA extern module *ap_top_module;

/**
 * Array of all statically linked modules
 * @defvar module *ap_prelinked_modules[]
 */
AP_DECLARE_DATA extern module *ap_prelinked_modules[];
/**
 * Array of all statically linked modulenames (symbols)
 * @defvar ap_module_symbol_t ap_prelinked_modulenames[]
 */
AP_DECLARE_DATA extern ap_module_symbol_t ap_prelinked_module_symbols[];
/**
 * Array of all preloaded modules
 * @defvar module *ap_preloaded_modules[]
 */
AP_DECLARE_DATA extern module *ap_preloaded_modules[];
/**
 * Array of all loaded modules
 * @defvar module **ap_loaded_modules
 */
AP_DECLARE_DATA extern module **ap_loaded_modules;

/* For mod_so.c... */
/** Run a single module's two create_config hooks
 *  @param p the pool to allocate from
 *  @param s The server to configure for.
 *  @param m The module to configure
 */
AP_DECLARE(void) ap_single_module_configure(apr_pool_t *p, server_rec *s, 
                                            module *m);

/* For http_main.c... */
/**
 * Add all of the prelinked modules into the loaded module list
 * @param process The process that is currently running the server
 */
AP_DECLARE(const char *) ap_setup_prelinked_modules(process_rec *process);

/**
 * Show the preloaded configuration directives, the help string explaining
 * the directive arguments, in what module they are handled, and in
 * what parts of the configuration they are allowed.  Used for httpd -h.
 */
AP_DECLARE(void) ap_show_directives(void);

/** 
 * Show the preloaded module names.  Used for httpd -l. 
 */
AP_DECLARE(void) ap_show_modules(void);

/** 
 * Show the MPM name.  Used in reporting modules such as mod_info to
 * provide extra information to the user
 */
AP_DECLARE(const char *) ap_show_mpm(void);

/**
 * Read all config files and setup the server
 * @param process The process running the server
 * @param temp_pool A pool to allocate temporary data from.
 * @param config_name The name of the config file
 * @param conftree Place to store the root of the config tree
 * @return The setup server_rec list.
 */
AP_DECLARE(server_rec *) ap_read_config(process_rec *process, 
                                        apr_pool_t *temp_pool, 
                                        const char *config_name, 
                                        ap_directive_t **conftree);

/**
 * Run all rewrite args hooks for loaded modules
 * @param process The process currently running the server
 */
AP_DECLARE(void) ap_run_rewrite_args(process_rec *process);

/**
 * Run the register hooks function for a specified module
 * @param m The module to run the register hooks function fo
 * @param p The pool valid for the lifetime of the module
 */
AP_DECLARE(void) ap_register_hooks(module *m, apr_pool_t *p);

/**
 * Setup all virtual hosts
 * @param p The pool to allocate from
 * @param main_server The head of the server_rec list
 */
AP_DECLARE(void) ap_fixup_virtual_hosts(apr_pool_t *p, 
                                        server_rec *main_server);

/* For http_request.c... */

/**
 * Setup the config vector for a request_rec
 * @param p The pool to allocate the config vector from
 * @return The config vector
 */
AP_CORE_DECLARE(ap_conf_vector_t*) ap_create_request_config(apr_pool_t *p);

/**
 * Setup the config vector for per dir module configs
 * @param p The pool to allocate the config vector from
 * @return The config vector
 */
AP_CORE_DECLARE(ap_conf_vector_t *) ap_create_per_dir_config(apr_pool_t *p);

/**
 * Run all of the modules merge per dir config functions
 * @param p The pool to pass to the merge functions
 * @param base The base directory config structure
 * @param new_conf The new directory config structure
 */
AP_CORE_DECLARE(ap_conf_vector_t*) ap_merge_per_dir_configs(apr_pool_t *p,
                                           ap_conf_vector_t *base,
                                           ap_conf_vector_t *new_conf);

/* For http_connection.c... */
/**
 * Setup the config vector for a connection_rec
 * @param p The pool to allocate the config vector from
 * @return The config vector
 */
AP_CORE_DECLARE(ap_conf_vector_t*) ap_create_conn_config(apr_pool_t *p);

/* For http_core.c... (<Directory> command and virtual hosts) */

/**
 * parse an htaccess file
 * @param resulting htaccess_result
 * @param r The request currently being served
 * @param override Which overrides are active
 * @param path The path to the htaccess file
 * @param access_name The list of possible names for .htaccess files
 * int The status of the current request
 */
AP_CORE_DECLARE(int) ap_parse_htaccess(ap_conf_vector_t **result, 
                                       request_rec *r, int override,
                                       int override_opts,
                                       const char *path, 
                                       const char *access_name);

/**
 * Setup a virtual host
 * @param p The pool to allocate all memory from
 * @param hostname The hostname of the virtual hsot
 * @param main_server The main server for this Apache configuration
 * @param ps Place to store the new server_rec
 * return Error string on error, NULL on success
 */
AP_CORE_DECLARE(const char *) ap_init_virtual_host(apr_pool_t *p, 
                                                   const char *hostname,
                                                   server_rec *main_server, 
                                                   server_rec **ps);

/**
 * Process the config file for Apache
 * @param s The server rec to use for the command parms
 * @param fname The name of the config file
 * @param conftree The root node of the created config tree
 * @param p Pool for general allocation
 * @param ptem Pool for temporary allocation
 */
AP_DECLARE(const char *) ap_process_resource_config(server_rec *s,
                                                    const char *fname,
                                                    ap_directive_t **conftree,
                                                    apr_pool_t *p,
                                                    apr_pool_t *ptemp);

/**
 * Process all directives in the config tree
 * @param s The server rec to use in the command parms
 * @param conftree The config tree to process
 * @param p The pool for general allocation
 * @param ptemp The pool for temporary allocations
 * @return OK if no problems
 */
AP_DECLARE(int) ap_process_config_tree(server_rec *s,
                                       ap_directive_t *conftree,
                                       apr_pool_t *p,
                                       apr_pool_t *ptemp);

/* Module-method dispatchers, also for http_request.c */
/**
 * Run the handler phase of each module until a module accepts the
 * responsibility of serving the request
 * @param r The current request
 * @return The status of the current request
 */
AP_CORE_DECLARE(int) ap_invoke_handler(request_rec *r);

/* for mod_perl */

/**
 * Find a given directive in a command_rec table
 * @param name The directive to search for
 * @param cmds The table to search
 * @return The directive definition of the specified directive
 */
AP_CORE_DECLARE(const command_rec *) ap_find_command(const char *name, 
                                                     const command_rec *cmds);

/**
 * Find a given directive in a list module
 * @param cmd_name The directive to search for
 * @param mod The module list to search
 * @return The directive definition of the specified directive
 */
AP_CORE_DECLARE(const command_rec *) ap_find_command_in_modules(const char *cmd_name, 
                                                                module **mod);

/**
 * Ask a module to create per-server and per-section (dir/loc/file) configs
 * (if it hasn't happened already). The results are stored in the server's
 * config, and the specified per-section config vector.
 * @param server The server to operate upon.
 * @param section_vector The per-section config vector.
 * @param section Which section to create a config for.
 * @param mod The module which is defining the config data.
 * @param pconf A pool for all configuration allocations.
 * @return The (new) per-section config data.
 */
AP_CORE_DECLARE(void *) ap_set_config_vectors(server_rec *server,
                                              ap_conf_vector_t *section_vector,
                                              const char *section,
                                              module *mod, apr_pool_t *pconf);

#endif

  /* Hooks */

/**
 * Run the header parser functions for each module
 * @param r The current request
 * @return OK or DECLINED
 */
AP_DECLARE_HOOK(int,header_parser,(request_rec *r))

/**
 * Run the pre_config function for each module
 * @param pconf The config pool
 * @param plog The logging streams pool
 * @param ptemp The temporary pool
 * @return OK or DECLINED on success anything else is a error
 */
AP_DECLARE_HOOK(int,pre_config,(apr_pool_t *pconf,apr_pool_t *plog,
                                apr_pool_t *ptemp))

/**
 * Run the test_config function for each module; this hook is run
 * only if the server was invoked to test the configuration syntax.
 * @param pconf The config pool
 * @param s The list of server_recs
 */
AP_DECLARE_HOOK(void,test_config,(apr_pool_t *pconf, server_rec *s))

/**
 * Run the post_config function for each module
 * @param pconf The config pool
 * @param plog The logging streams pool
 * @param ptemp The temporary pool
 * @param s The list of server_recs
 * @return OK or DECLINED on success anything else is a error
 */
AP_DECLARE_HOOK(int,post_config,(apr_pool_t *pconf,apr_pool_t *plog,
                                 apr_pool_t *ptemp,server_rec *s))

/**
 * Run the open_logs functions for each module
 * @param pconf The config pool
 * @param plog The logging streams pool
 * @param ptemp The temporary pool
 * @param s The list of server_recs
 * @return OK or DECLINED on success anything else is a error
 */
AP_DECLARE_HOOK(int,open_logs,(apr_pool_t *pconf,apr_pool_t *plog,
                               apr_pool_t *ptemp,server_rec *s))

/**
 * Run the child_init functions for each module
 * @param pchild The child pool
 * @param s The list of server_recs in this server 
 */
AP_DECLARE_HOOK(void,child_init,(apr_pool_t *pchild, server_rec *s))

/**
 * Run the handler functions for each module
 * @param r The request_rec
 * @remark non-wildcard handlers should HOOK_MIDDLE, wildcard HOOK_LAST
 */
AP_DECLARE_HOOK(int,handler,(request_rec *r))

/**
 * Run the quick handler functions for each module. The quick_handler
 * is run before any other requests hooks are called (location_walk,
 * directory_walk, access checking, et. al.). This hook was added
 * to provide a quick way to serve content from a URI keyed cache.
 * 
 * @param r The request_rec
 * @param lookup_uri Controls whether the caller actually wants content or not.
 * lookup is set when the quick_handler is called out of 
 * ap_sub_req_lookup_uri()
 */
AP_DECLARE_HOOK(int,quick_handler,(request_rec *r, int lookup_uri))

/**
 * Retrieve the optional functions for each module.
 * This is run immediately before the server starts. Optional functions should
 * be registered during the hook registration phase.
 */
AP_DECLARE_HOOK(void,optional_fn_retrieve,(void))

#ifdef __cplusplus
}
#endif

#endif	/* !APACHE_HTTP_CONFIG_H */
/** @} */
