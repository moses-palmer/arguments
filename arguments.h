#include <stdio.h>
#include <stdlib.h>

#include <memory.h>

/**
 * This is the macro used to define command line parameters. Create the file
 * arguments.def and populate it with invocations of this macro.
 *
 * The order of the arguments found in arguments.def needs to reflect the
 * parameters to to the start function in start.c if ARGUMENTS_AUTOMATIC is
 * defined.
 *
 * @param type
 *     The type of the variable that stores the parsed value of this argument.
 *
 * @param name
 *     The name of the variable, and also the long argument name. The long
 *     argument name is constructed by prepending two dashes to the name and
 *     then replacing all underscores with dashes.
 *
 * @param short
 *     The short command line argument name. This may be NULL.
 *
 * @param value_count
 *     The number of parameters following the named parameter that are required.
 *
 * @param is_required
 *     Whether this argument is required on the command line. The value for
 *     is_required is evaluated once all arguments have been read, so it may
 *     depend on arguments passed on the command line.
 *
 * @param help
 *     The help text for the argument. The first occurrence of "%s" in this
 *     message will be replaced by the default value.
 *
 * @param set_default
 *     A list of statements executed when the argument has not been passed on
 *     the command line. The following variables are available to the code:
 *       - type *target: a pointer to the variable to receive the value.
 *     If there is not suitable default value for the argument, pass
 *     ARGUMENTS_NO_DEFAULT.
 *
 * @param read
 *     A list of statements used to actually set the value of the variable. The
 *     following variables are available to the code:
 *       - type *target: a pointer to the variable to receive the value.
 *       - const char *value: the actual value passed.
 *       - int is_valid: whether the value passed was valid; set this to 0 if
 *         the argument variable could not be initialised.
 *
 * @param release
 *     A list of statements used to release any resources used by the variable;
 *     this may be closing files, or freeing allocated memory. The following
 *     variables are available to the code:
 *       - type *target: a pointer to the variable that received the value.
 */
#define _A(type, name, short, value_count, is_required, help, \
    set_default, read, release)

/**
 * Pass this value as short if the argument does not have a short name.
 */
#define ARGUMENT_NO_SHORT_OPTION NULL

/**
 * Pass this value as is_required to the _A macro if the argument is required.
 */
#define ARGUMENT_IS_REQUIRED 1

/**
 * Pass this value as is_required to the _A macro if the argument is not
 * required.
 */
#define ARGUMENT_IS_OPTIONAL 0

/**
 * Checks that a command line argument has been passed.
 */
#define ARGUMENT_IS_PRESENT(name) \
    arguments.name.present

/**
 * Reads the value of an argument
 */
#define ARGUMENT_VALUE(name) \
    arguments.name.value

/**
 * We include arguments.def to define anything in the ARGUMENTS_HELPERS section
 * and the types of the arguments.
 */
#undef _A
#define _A(type, name, short, value_count, is_required, help, \
        set_default, read, release) \
    typedef type name##_t;
#include "arguments.def"

/**
 * Whether to use this file in automatic mode.
 */
#ifndef ARGUMENTS_AUTOMATIC
    #define ARGUMENTS_AUTOMATIC 1
#endif

/**
 * Whether to always print help if the --help argument is passed.
 *
 * If ARGUMENTS_HELP is also non-zero, it is used as a string parameter to
 * printf.
 */
#ifndef ARGUMENTS_PRINT_HELP
    #define ARGUMENTS_PRINT_HELP 1
#endif

/**
 * The return code when an invalid command line parameter is encountered.
 */
#ifndef ARGUMENTS_PARAMETER_INVALID
    #define ARGUMENTS_PARAMETER_INVALID 110
#endif

/**
 * The return code when a required command line parameter is missing.
 */
#ifndef ARGUMENTS_PARAMETER_MISSING
    #define ARGUMENTS_PARAMETER_MISSING 120
#endif


#if ARGUMENTS_AUTOMATIC

/**
 * This function is called before the presence of all required arguments has
 * been verified, but before any arguments are converted to actual values.
 *
 * Use this function to initialise any external resources that are required by
 * the argument readers.
 *
 * If this function does not return 0, the program is terminated with the exit
 * code reurned by this function.
 *
 * If you do not need any setup, define ARGUMENTS_NO_SETUP.
 */
static int
setup(int argc, char *argv[])
#ifdef ARGUMENTS_NO_SETUP
{}
#else
;
#endif

/**
 * This is the function called by main.c once the arguments have been parsed if
 * ARGUMENTS_AUTOMATIC is non-zero.
 *
 * Its function signature matches that of main with all arguments found in
 * arguments.def added to the argument list as well.
 */
#undef _A
#define _A(type, name, short, value_count, is_required, help, \
        set_default, read, release) \
    , name##_t name
static int
run(int argc, char *argv[]
    #include "arguments.def"
    );

/**
 * This function is called before the application terminates if setup has been
 * called.
 *
 * If you do not need any teardown, define ARGUMENTS_NO_TEARDOWN.
 */
static void
teardown(void)
#ifdef ARGUMENTS_NO_TEARDOWN
{}
#else
;
#endif

#endif


/**
 * Return values used by arguments_validate and arguments_parse.
 */
enum {
    /**
     * The operation completed successfully.
     */
    AC_OK,

    /**
     * An invalid value was passed on the command line, or a required argument
     * was missing.
     */
    AC_ERROR,

    /**
     * The command line argument --help was found, and the help message was
     * printed.
     *
     * Unless ARGUMENTS_PRINT_HELP is non-zero, this value will not be returned.
     */
    AC_HELP
};


/**
 * Compares a long argument to a name string.
 *
 * The first two characters of longarg must be "--".
 *
 * @param longarg
 *     The long argument.
 * @param name
 *     The name of the argument.
 * @return 0 if longarg is the long argument for name, non-zero otherwise
 */
static int
arguments_cmp(const char *longarg, const char *name)
{
    /* Skip the first two characters */
    if (*(longarg++) != '-') return -1;
    if (*(longarg++) != '-') return -1;

    /* Compare the rest of the characters one by one, converting '_' in name to
       '-' */
    while (*longarg && *name) {
        if (*longarg != (*name == '_' ? '-' : *name)) {
            break;
        }
        longarg++;
        name++;
    }

    return *longarg - (*name == '_') ? '-' : *name;
}


/**
 * The struct that contains all argument values.
 *
 * For every argument, a single struct is created containing
 *   * present: whether this argument was present on the command line
 *   * initialized: whether this argument has been initialised
 *   * value_strings: the value passed for the argument on the command line, if
 *     any
 *   * value_strings_length: the number of string values in value_strings
 *   * value: its value after it has been parsed
 */
static struct {
#undef _A
#define _A(type, name, short, value_count, is_required, help, \
        set_default, read, release) \
    struct { \
        int present; \
        int initialized; \
        char **value_strings; \
        unsigned int value_strings_length; \
        name##_t value; \
    } name;
#include "arguments.def"
} arguments;


/**
 * Initialises the arguments struct by zeroing it.
 *
 * Unless ARGUMENTS_AUTOMATIC is non-zero, this should be called before calling
 * arguments_parse.
 */
static void
arguments_initialize(void)
{
    memset(&arguments, 0, sizeof(arguments));
}


#if ARGUMENTS_PRINT_HELP || ARGUMENTS_AUTOMATIC
    #include "arguments-help.h"
#endif


/**
 * Releases all arguments that have been initialised.
 *
 * If arguments_set has been called, this function must also be called, even if
 * arguments_set returned an error.
 */
static void
arguments_release(void)
{
#undef _A
#define _A(type, name, short, value_count, is_required, help, \
        set_default, read, release) \
    if (arguments.name.initialized) { \
        name##_t *target = &arguments.name.value; \
        \
        do { \
            release \
        } while (0); \
        \
        if (target); \
    }
#include "arguments.def"
}


/**
 * Parses the command line given by argv and argc.
 *
 * If ARGUMENTS_AUTOMATIC is not defined, this function will simply parse the
 * arguments from argv[*nextarg] until either the end of argv is reached or an
 * invalid argument value is encountered.
 *
 * If ARGUMENTS_PRINT_HELP is non-zero, this function will print a help message
 * if the arguments --help or -h are encountered. If ARGUMENTS_AUTOMATIC is also
 * non-zero, this function will not return; rather, it will terminate the
 * process with the return code 0.
 *
 * If ARGUMENTS_AUTOMATIC is non-zero, this function will automatically
 * initialise the argument struct and then parse all arguments. If an invalid
 * argument value is encountered, it will not return; rather, it will terminate
 * the process with the return code 1.
 *
 * @param argv
 *     The argument vector, typically the same as argv of the standard main
 *     function.
 * @param argc
 *     The number of elements in argv.
 * @param nextarg
 *     The index of the first argument to examine. Upon return, the int pointed
 *     to by this argument will contain the index of the next unparsed argument.
 *     If nextarg == argc, all arguments were parsed. If an error occurred
 *     while parsing an argument, nextarg will contain its index.
 * @return AC_OK if no error occurred, AC_HELP if --help was encountered and
 *     ARGUMENTS_PRINT_HELP was non-zero, or AC_INVALID if an invalid value was
 *     passed
 */
static int
arguments_read(int argc, char *argv[], int *nextarg)
{
    int is_valid = 1;

    if (*nextarg < 1) {
        *nextarg = 1;
    }

    /* Parse the command line */
    while (*nextarg < argc && is_valid) {
        /* If ARGUMENTS_PRINT_HELP is defined, we handle --help and -h */
#if ARGUMENTS_PRINT_HELP
        if ((strcmp(argv[*nextarg], "--help") == 0)
                || (strcmp(argv[*nextarg], "-h") == 0)) {
            arguments_print_help();
            return AC_HELP;
        }
#else
        /* If ARGUMENTS_PRINT_HELP is zero, we need a dummy if statement to
           allow the following list of else ifs */
        if (0);
#endif

    #undef _A
    #define _A(type, name, short, value_count, is_required, help, \
            set_default, read, release) \
        else if ((arguments_cmp(argv[*nextarg], #name) == 0) \
                || (short && strcmp(argv[*nextarg], short) == 0)) { \
            arguments.name.present = 1; \
            arguments.name.value_strings_length = value_count; \
            \
            if (*nextarg + arguments.name.value_strings_length < argc) { \
                (*nextarg)++; \
                arguments.name.value_strings = argv + *nextarg; \
                *nextarg += arguments.name.value_strings_length; \
            } \
            else { \
                is_valid = 0; \
                break; \
            } \
        }
    #include "arguments.def"

        else {
            /* If no argument matched the current one, break */
            break;
        }
    }

    /* is_valid is non-zero unless a command line parameter is missing */
    return is_valid ? AC_OK : AC_ERROR;
}

/**
 * Call this function after all invocations of arguments_read.
 *
 * It will set the default values for all arguments that have not been passed on
 * the command line. If an argument that was required has not been passed, this
 * function will return AC_ERROR.
 *
 * @return AC_OK if all required arguments have been passed, or AC_ERROR if a
 *     parameter error is encountered
 */
static int
arguments_set(void)
{
    int is_valid = 1;

#undef _A
#define _A(type, name, short, value_count, is_required, help, \
        set_default, read, release) \
    if (is_valid) { \
        name##_t *target = &arguments.name.value; \
        char **value_strings = \
            arguments.name.value_strings; \
        unsigned int value_strings_length = \
            arguments.name.value_strings_length; \
        \
        if (arguments.name.present) { \
            read \
        } \
        else { \
            set_default \
        } \
        \
        if (is_valid) { \
            arguments.name.initialized = 1; \
        } \
        \
        if (target); \
        if (value_strings); \
        if (value_strings_length); \
    }
#include "arguments.def"

    /* This function has to be called */
    atexit(arguments_release);

    return is_valid ? AC_OK : AC_ERROR;
}


/*
 * If ARGUMENTS_AUTOMATIC is non-zero, we implement main() and call start()
 * instead.
 */
#if ARGUMENTS_AUTOMATIC

int
main(int argc, char *argv[])
{
    int result;
    int nextarg = 1;

    arguments_initialize();

    /* First parse the arguments */
    switch (arguments_read(argc, argv, &nextarg)) {
    case AC_OK:
        /* Just continue if no invalid arguments were found */
        break;

    case AC_HELP:
        return 0;

    case AC_ERROR:
        /* A parameter requiring values was not passed enough values before the
           command line ended */
        return ARGUMENTS_PARAMETER_INVALID;
    }

    /* Detect missing arguments */
#ifdef ARGUMENTS_PRINT_MISSING_FORMAT
    #define print_missing(name) \
        fprintf(stderr, ARGUMENTS_PRINT_MISSING_FORMAT, name)
#else
    #define print_missing(name)
#endif

#undef _A
#define _A(type, name, short, value_count, is_required, help, \
        set_default, read, release) \
    if ((is_required) && !arguments.name.present) { \
        print_missing(#name); \
        return ARGUMENTS_PARAMETER_MISSING; \
    }
#include "arguments.def"

    /* Call setup before converting the parameter values to variables */
    result = setup(argc, argv);
    if (result != 0) {
        return result;
    }

    /* Once we have successfully called setup, we must make sure that teardown
       is also called */
    atexit(teardown);

    /* Actually converts the string values read to variables */
    switch (arguments_set()) {
    case AC_OK:
        /* Just continue if no invalid arguments were found */
        break;

    default:
        /* A parameter requiring values was not passed enough values before the
           command line ended */
        return ARGUMENTS_PARAMETER_INVALID;
    }

    return run(argc, argv
        #undef _A
        #define _A(type, name, short, value_count, is_required, help, \
                set_default, read, release) \
            , arguments.name.value
        #include "arguments.def"
    );
}


#define main run

#endif
