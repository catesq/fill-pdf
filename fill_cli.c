#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <jansson.h>
#include <unistd.h>
#include <ctype.h>

#include "fill.h"

const char *command_names[CMD_COUNT] = {
    "annot", "info", "template", "fonts", "complete"
};

void usage_message(int cmd) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  fillpdf <command> [options] input.pdf [output]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Available commands:\n");
    fprintf(stderr, "  annot info template fonts complete\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");

    if(cmd != COMPLETE_PDF) {
        fprintf(stderr, "  fillpdf annot input.pdf [output.pdf]\n");
        fprintf(stderr, "      [output.pdf] defaults to the input filename suffixed with '_annotated.pdf'.\n");
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
}


int read_parse_cmd_args(int argc, char **argv, pdf_env *env) {
    env->files.input = argv[2];

    if(argc == 4) {
        env->files.output = argv[3];
    }

    return 1;
}

int read_completion_cmd_args(int argc, char **argv, pdf_env *env) {
    int arg;

    argc--;
    argv++;

    while((arg = getopt(argc, argv, "t:d:s:p:")) != -1) {
        switch(arg) {
        case 't':
            env->fill.tplFile = optarg;
            break;

        case 'd':
            env->fill.dataFile = optarg;
            break;

        case 's':
            env->fill.certFile = optarg;
            break;

        case 'p':
            env->fill.certPwd = optarg;
            break;
        }
    }

    if(optind < argc) {
        env->files.input = argv[optind];
    } else {
        fprintf(stderr, "Error: Input filename missing\n\n");
        return 0;
    }

    optind++;

    if(optind < argc) {
        env->files.output = argv[optind];
    }

    return 1;
}


command get_command(char *cmd) {
    int i;

    for(i=0; i<CMD_COUNT; i++) {
        if(strncmp(command_names[i], cmd, strlen(command_names[i])) == 0)
            return i;
        else if(strlen(cmd) == 1 && *cmd == *command_names[i])
            return i;
    }

    return -1;
}

const char *command_name(command cmd) {
    if(cmd >= 0 && cmd < CMD_COUNT) {
        return command_names[cmd];
    } else {
        return NULL;
    }
}

int read_args(int argc, char **argv, pdf_env *env) {
    if(argc == 1) {
        fprintf(stderr, "Error: No sub-command, or input file, given.\n\n");
        return 0;
    }

    env->cmd = get_command(argv[1]);

    if(env->cmd == -1) {
        fprintf(stderr, "Error: '%s' is not a recognised command.\n\n", argv[1]);
        return 0;
    } else if (argc == 2) {
        fprintf(stderr, "Error: No input file given.\n\n");
        return 0;
    }

    if(env->cmd == COMPLETE_PDF) {
        return read_completion_cmd_args(argc, argv, env);;
    } else {
        return read_parse_cmd_args(argc, argv, env);
    }
}

int main(int argc, char **argv) {
    const char *command;
    int caught_err = 0;
    int retval = EXIT_SUCCESS;

    pdf_env *env = malloc(sizeof(pdf_env));
    memset(env, 0, sizeof(pdf_env));

    if(!read_args(argc, argv, env)) {
        usage_message(env->cmd);
        retval = EXIT_FAILURE;
        goto main_exit;
    }

    env->ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);

    if (!env->ctx) {
        fprintf(stderr, "cannot create mupdf context\n");
        retval = EXIT_FAILURE;
        goto main_exit;
    }

    /* Only handle pdf. */
    fz_try(env->ctx) {
        fz_register_document_handler(env->ctx, &pdf_document_handler);
    } fz_catch(env->ctx) {
        fprintf(stderr, "cannot register document handlers: %s\n", fz_caught_message(env->ctx));
        retval = EXIT_FAILURE;
    }

    if(retval == EXIT_FAILURE) goto main_exit_ctxt;

    /* Open the document. */
    fz_try(env->ctx) {
        env->doc = pdf_open_document(env->ctx, env->files.input);
    } fz_catch(env->ctx)	{
        fprintf(stderr, "cannot open document: %s\n", fz_caught_message(env->ctx));
        retval = EXIT_FAILURE;
    }

    if(retval == EXIT_FAILURE) goto main_exit_ctxt;

    /* Count the number of pages. */
    fz_try(env->ctx) {
        env->page_count = pdf_count_pages(env->ctx, env->doc);
    } fz_catch(env->ctx)	{
        pdf_drop_document(env->ctx, env->doc);
        fprintf(stderr, "cannot count number of pages: %s\n", fz_caught_message(env->ctx));
        retval = EXIT_FAILURE;
    }

    if(retval == EXIT_FAILURE) goto main_exit_ctxt;

    if(env->cmd == COMPLETE_PDF) {
        cmplt_fill_all(env);
    } else {
        parse_fields_doc(env);
        pdf_drop_document(env->ctx, env->doc);
    }


main_exit_ctxt:
    fz_drop_context(env->ctx);
main_exit:
    return retval;
}
