#include <ctype.h>
#include <wchar.h>

/**
 * Returns the width of the terminal.
 *
 * @return the width of the terminal, or (unsigned int)-1 if it cannot be
 *   determined
 */
static unsigned int
arguments_terminal_width(void)
{
    char *columns = getenv("COLUMNS");
    int result = columns ? atoi(columns) : 80;

    if (!result) {
        result = (unsigned int)-1;
    }

    return result;
}

/**
 * Returns the number of characters needed for the argument names header.
 *
 * @return the number of character needed for the argument names header
 */
static unsigned int
argument_header_width(void)
{
    unsigned int current;
    unsigned int result;

    result = 0;

    /* Calculate how many characters we need for the argument names */
#undef ARGUMENT
#define ARGUMENT(type, name, short, value_count, is_required, help, \
        set_default, read, release) \
    current = 2 + strlen(#name); \
    if (short) { \
        current += 2 + strlen(short ? short : ""); \
    } \
    if (current > result) { \
        result = current; \
    }
#include "../arguments.def"

    return result;
}

#define isend(c) \
    (((c) == 0) || ((c) == '\n'))

/**
 * Determines the number of characters starting at s to print and the offset to
 * the next line.
 *
 * @param s
 *     The string to investigate. *s has to point to the first character of a
 *     line.
 * @param length
 *     The number of characters to print is written to this variable.
 * @param max_width
 *     The maximum number of characters to print.
 * @return the offset to the beginning of the next line
 *
 */
static unsigned int
arguments_get_line(const char *s, unsigned int *length,
    unsigned int max_length)
{
    mbstate_t mbs;
    int was_space, seen_space;
    unsigned int word_count, end, l, i, result;

    /* Initialise the multibyte state */
    memset(&mbs, 0, sizeof(mbs));
    if (!mbsinit(&mbs)) {
        char outbuf[8];
        const wchar_t empty[] = L"";
        const wchar_t *srcp = empty;
        wcsrtombs(outbuf, &srcp, sizeof(outbuf), &mbs);
    }

    *length = 0;
    was_space = 0;
    seen_space = 0;
    word_count = 0;
    end = strlen(s);
    l = 0;
    i = 0;
    result = 0;

    for (i = 0; (i < end) && (l < max_length);) {
        int is_space;
        size_t di;

        /* Break immediately if we reach newline */
        if (s[i] == '\n') {
            *length = l;
            result = i + 1;
            break;
        }

        if (!seen_space) {
            /* If we have not yet encountered any space, we have to update the
               length of the line, since the word will then have to be squeezed
               in */
            *length = l;
            result = i;
        }

        /* We are processing a character; increase the length */
        l++;

        is_space = isspace(s[i]);
        seen_space |= is_space;
        if (is_space && !was_space) {
            /* If the current character is space following non-space, the
               previous character was the last character of a word and we update
               the length */
            *length = l - 1;
        }
        else if (!is_space && was_space) {
            /* If this is a non-space character following a space character,
               this is the first character of a word and we update the offset of
               the next line */
            result = i;
        }
        was_space = is_space;

        /* Calculate how many bytes we need to increment our position for the
           current character */
        di = mbrlen(s + i, end - i, &mbs);
        switch (di) {
        case 0:
            /* This should really not happen */
            break;

        case (size_t)-1:
            /* Invalid character sequence; skip to the next byte */
            i++;
            break;

        case (size_t)-2:
            /* Possibly incomplete multi byte sequence; skip to the next
               byte */
            i++;
            break;

        default:
            i += di;
        }

        /* If this is the last character of the string, we update length and
           result to make sure that all characters are included and the caller
           knows that the string has been terminated */
        if (i == end) {
            *length = l;
            result = i;
        }
    }

    /* If result is less than length, we have stopped before reaching a new
       word */
    if (result < *length) {
        result = *length;
    }

    /* Skip any trailing space so that the next offset will be the start of a
       word or the end of the string */
    while ((result < end) && (s[result] == ' ')) result++;

    return result;
}

/**
 * Prints the help for a single argument.
 *
 * @param header
 *     The argument header. This should be "--long-name" or "--long-name, -s".
 *     This parameter is optional and may be NULL if only the help should be
 *     printed. In that case, header_width is set to 0.
 * @param help
 *     The help string.
 * @param header_width
 *     The width of the header column.
 * @param terminal_width
 *     The width of the terminal.
 */
static void
arguments_print_help_string(const char *header, const char *help,
    unsigned int header_width, unsigned int terminal_width)
{
    const char *c;

    /* Print the header, which is the argument names left aligned and padded up
       to names_width characters */
    if (header) {
        printf("\n%-*s ", header_width, header);
    }
    else {
        header_width = 0;
    }

    c = help;
    while (*c) {
        unsigned int n, length;

        /* Get the length of the current line, and the offset of the start of
           the next line */
        n = arguments_get_line(c, &length, terminal_width - header_width
            + (header ? 0 : 1));

        /* Print at most length charaters from the current offset in the help
           string */
        printf("%-.*s", length, c);

        /* Do not print newline when the string covers the entire line */
        if (length < terminal_width - header_width - (header ? 1 : 0)) {
            printf("\n");
        }
        c += n;

        /* If we have not reached the end of the help string, print a new empty
           header column; do not do this if no header has been specified */
        if (*c && header) {
            unsigned int i;

            for (i = 0; i <= header_width; i++) {
                putc(' ', stdout);
            }
        }
    }
}

/**
 * Prints the help for all commands.
 */
static void
arguments_print_help(void)
{
    unsigned int terminal_width;
    int header_width;
    char header[128], *h;

    /* Determine the width of the termihnal */
    terminal_width = arguments_terminal_width();

    /* Determine how many columns we need for the argument names */
    header_width = argument_header_width();

#ifdef ARGUMENTS_HELP
    /* Print the help header */
    arguments_print_help_string(NULL, ARGUMENTS_HELP, 0, terminal_width);
#endif

    /* Print the argument help strings */
#undef ARGUMENT
#define ARGUMENT(type, name, short, help, value_count, is_required, \
        set_default, read, release) \
    if (short) { \
        snprintf(header, sizeof(header), "--%s, %s", #name, \
            short ? short : ""); \
    } \
    else { \
        snprintf(header, sizeof(header), "--%s", #name); \
    } \
    for (h = header; *h; h++) { \
        if (*h == '_') { \
            *h = '-'; \
        } \
    } \
    arguments_print_help_string(header, help, header_width, terminal_width);
#include "../arguments.def"
}
