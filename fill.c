#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <jansson.h>
#include <unistd.h>
#include <ctype.h>

#include "fill.h"

extern fz_document_handler pdf_document_handler;

#define CMD_COUNT 5

const char *command_names[CMD_COUNT] = {
    "annot", "info", "template", "fonts", "complete"
};

static void usage_message(int cmd) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  fillpdf <command> [options] input.pdf [output]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Available <command>s:\n");
    fprintf(stderr, "  annot info template fonts complete\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");

    if(cmd != COMPLETE_PDF) {
        fprintf(stderr, "  fillpdf annot input.pdf [output.pdf]\n");
        fprintf(stderr, "      [output.pdf] defau;ts to the input filename suffixed with '_annotated.pdf'.\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "  fillpdf info input.pdf [info.json]\n");
        fprintf(stderr, "  fillpdf template input.pdf [template.json]\n");
        fprintf(stderr, "  fillpdf fonts input.pdf [fontlist.json]\n");
        fprintf(stderr, "      [output.json] all default to stdout.\n");
        fprintf(stderr, "\n");
    }

    if(cmd == COMPLETE_PDF || cmd == -1) {
        fprintf(stderr, "  fillpdf complete [-t tpl.json] [-s cert.pfx] [-p passwd] [-d data.json] input.pdf [output.pdf]\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Options for 'complete':\n");
        fprintf(stderr, "  -t tpl.json   The template maps input data to pdf fields.\n");
        fprintf(stderr, "  -d data.json  Input data in json file.\n");
        fprintf(stderr, "  -s cert.pfx   Certificate to sign pdf.\n");
        fprintf(stderr, "  -p password   Password for cert.pfx.\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Notes for 'complete':\n");
        fprintf(stderr, "  If -t option not given then a template file is expected\n");
        fprintf(stderr, "  in the same dir as 'inputfile.pdf' with json file type ie 'inputfile.json'\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "  If -d option missing then stdin is used\n");
        fprintf(stderr, "  The -s & -d options may be defined in the template, on the command line or unused\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "  output.pdf defaults to an auto-generated filename based on the input filename.\n");
        fprintf(stderr, "\n");
    }

    fprintf(stderr, "Other notes:\n");


}


static int read_info_cmd_args(int argc, char **argv, pdf_env *env) {
    env->files.input = argv[2];

    if(argc == 4) {
        env->files.output = argv[3];
    }

    return 1;
}

static int read_completion_cmd_args(int argc, char **argv, pdf_env *env) {
    int arg;

    argc--;
    argv++;

    while((arg = getopt(argc, argv, "t:d:s:p")) != -1) {
        switch(arg) {
        case 't':
            env->fill.tplFile = optarg;
            break;

        case 'd':
            env->fill.dataFile = optarg;
            break;

        case 's':
            env->fill.sig.file = optarg;
            break;

        case 'p':
            env->fill.sig.password = optarg;
            break;
        }
    }

    if(optind < argc) {
        env->files.input = argv[optind];
    } else {
        fprintf(stderr, "Error: Input filename missing\n\n");
        return EXIT_FAILURE;
    }

    optind++;

    if(optind < argc) {
        env->files.output = argv[optind];
    }

    return EXIT_SUCCESS;
}


static command get_command(char *cmd) {
    int i;
    for(i=0; i<CMD_COUNT; i++) {
        if(strncmp(command_names[i], cmd, strlen(command_names[i])) == 0)
            return i;
        else if(strlen(cmd) == 1 && *cmd == *command_names[i])
            return i;
    }

    return -1;
}

static int read_args(int argc, char **argv, pdf_env *env) {
    if(argc == 1) {
        fprintf(stderr, "Error: No sub-command, or input file, given.\n\n");
        usage_message(-1);
        return EXIT_FAILURE;
    }

    env->cmd = get_command(argv[1]);

    if (argc == 2) {
        fprintf(stderr, "Error: No input file given.\n\n");
        usage_message(env->cmd);
        return EXIT_FAILURE;
    }

    switch(env->cmd) {
        case -1:
            return EXIT_FAILURE;

        case COMPLETE_PDF:
            return read_completion_cmd_args(argc, argv, env);;

        default:
            return read_info_cmd_args(argc, argv, env);
    }
}

int main(int argc, char **argv) {
    const char *command;
    int caught_err = 0;
    int retval;

    pdf_env *env = malloc(sizeof(pdf_env));
    memset(env, 0, sizeof(pdf_env));

    if(!(retval = read_args(argc, argv, env))) {
        usage_message(env->cmd);
        goto main_exit;
    }

    env->ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);

    if (!env->ctx)	{
        fprintf(stderr, "cannot create mupdf context\n");
        goto main_exit;
    }

    /* Only handle pdf. */
    fz_try(env->ctx) {
        fz_register_document_handler(env->ctx, &pdf_document_handler);
    } fz_catch(env->ctx)	{
        fprintf(stderr, "cannot register document handlers: %s\n", fz_caught_message(env->ctx));
        caught_err = 1;
    }

    if(caught_err) goto main_exit_ctxt;

    /* Open the document. */
    fz_try(env->ctx) {
        env->doc = pdf_open_document(env->ctx, env->files.input);
    } fz_catch(env->ctx)	{
        fprintf(stderr, "cannot open document: %s\n", fz_caught_message(env->ctx));
        caught_err = 1;
    }

    if(caught_err) goto main_exit_ctxt;

    /* Count the number of pages. */
    fz_try(env->ctx) {
        env->page_count = pdf_count_pages(env->ctx, env->doc);
    } fz_catch(env->ctx)	{
        fprintf(stderr, "cannot count number of pages: %s\n", fz_caught_message(env->ctx));
        caught_err = 1;
    }

    if(caught_err) goto main_exit_doc;

    switch(env->cmd) {
        case COMPLETE_PDF: {
            cmplt_fill_all(env);
            break;
        }

        default:
            parse_fields_doc(env);
            break;
    }

main_exit_doc:
    pdf_drop_document(env->ctx, env->doc);
main_exit_ctxt:
    fz_drop_context(env->ctx);

main_exit:
    return retval;
}
