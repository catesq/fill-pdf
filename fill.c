#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <jansson.h>
#include <unistd.h>

#include "fill.h"

extern fz_document_handler pdf_document_handler;


int parse_args(int argc, char **argv, pdf_env *env) {
    int arg;
    while((arg = getopt(argc, argv, "f:d:m:s:p:j:til")) != -1) {
        switch(arg) {
        case 'f':
            if(env->cmd == NO_CMD) {
                env->cmd = FIELD_OVERLAY;
                env->optFile = optarg;
            }
            break;

        case 's':
            env->sigFile = optarg;
            break;

        case 'p':
            env->sigPwd = optarg;
            break;

        case 'j':
            env->optFile = optarg;
        case 't':
            if(env->cmd == NO_CMD) {
                env->cmd = JSON_MAP;
            }
            break;

        case 'l':
            if(env->cmd == NO_CMD) {
                env->cmd = JSON_LIST;
            }
            break;

        case 'm':
            env->optFile = optarg;
            break;

        case 'd':
            if(env->cmd == NO_CMD) {
                env->dataFile = optarg;
                env->cmd = COMPLETE_PDF;
            }
            break;

        case 'i':
            if(env->cmd == NO_CMD) {
                env->cmd = COMPLETE_PDF_STDIN;
            }
            break;
        }

    }

    if(optind < argc) {
        env->inputPdf = argv[optind];
    } else {
        return EXIT_FAILURE;
    }

    if(env->cmd == COMPLETE_PDF || env->cmd == COMPLETE_PDF_STDIN) {
        if(!optind + 1 >= argc) {
            return EXIT_FAILURE;
        } else {
            env->outputPdf = argv[optind+1];
        }
    }

    return env->cmd == NO_CMD ? EXIT_FAILURE : EXIT_SUCCESS;
}


int main(int argc, char **argv) {
    pdf_env env = {0};

    int retval = parse_args(argc, argv, &env);

    if (retval == EXIT_FAILURE) {
        fprintf(stderr, "Usage message, either: fillpdf [-f out.pdf|-j|-t|-l] input.pdf\n");
        fprintf(stderr, "                   or: fillpdf [-d data.json|-m tpl.json|-i|-s sigfile.pfx|-p pfx_pwd] input.pdf formfilled.pdf\n");
        fprintf(stderr, "Options:\n");
        fprintf(stderr, " -f output.pdf  Field name annotations. Save a pdf with each input fields object id\n");
        fprintf(stderr, "                displayed next to input field. Useful for filling in the template.\n");
        fprintf(stderr, " -j tpl.json    Write the part-completed json template to a file.\n");
        fprintf(stderr, " -t             Write the part-completed json template to stdout.\n");
        fprintf(stderr, " -l             List input fields and some info about them.\n");

        fprintf(stderr, "\n");
        fprintf(stderr, " -m tpl.json    The template maps input data to pdf fields.\n");
        fprintf(stderr, " -d data.json   Input data in json file.\n");
        fprintf(stderr, " -i             Input data read from stdin.\n");
        fprintf(stderr, " -s sigfile.pfx To sign pdf.\n");
        fprintf(stderr, " -p             Password for sigfile.pfx.\n");

        goto main_exit;
    }

    env.ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);

    if (!env.ctx)	{
        fprintf(stderr, "cannot create mupdf context\n");
        goto main_exit;
    }

    /* Only handle pdf. */
    fz_try(env.ctx) {
        fz_register_document_handler(env.ctx, &pdf_document_handler);
    } fz_catch(env.ctx)	{
        fprintf(stderr, "cannot register document handlers: %s\n", fz_caught_message(env.ctx));
        goto main_exit_ctxt;
    }

    /* Open the document. */
    fz_try(env.ctx) {
        env.doc = pdf_open_document(env.ctx, env.inputPdf);
    } fz_catch(env.ctx)	{
        fprintf(stderr, "cannot open document: %s\n", fz_caught_message(env.ctx));
        goto main_exit_ctxt;
    }

    /* Count the number of pages. */
    fz_try(env.ctx) {
        env.page_count = pdf_count_pages(env.ctx, env.doc);
    } fz_catch(env.ctx)	{
        fprintf(stderr, "cannot count number of pages: %s\n", fz_caught_message(env.ctx));
        goto main_exit_doc;
    }

    switch(env.cmd) {
        case COMPLETE_PDF: {
            FILE *infile = fopen(env.dataFile, "r");
            cmplt_fill_all(&env, infile);
            fclose(infile);
            break;
        }

        case COMPLETE_PDF_STDIN:
            cmplt_fill_all(&env, stdin);
            break;

        default:
            parse_fields_doc(&env);
            break;
    }

main_exit_doc:
    pdf_drop_document(env.ctx, env.doc);
main_exit_ctxt:
    fz_drop_context(env.ctx);

main_exit:
    return retval;
}
