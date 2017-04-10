#ifndef PDF_FILL_H
#define PDF_FILL_H

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <jansson.h>

typedef enum {FILL_DATA_INVALID, FIELD_ID, FIELD_NAME, ADD_TEXTFIELD, ADD_SIGNATURE} fill_type;

typedef enum {NO_CMD, FIELD_OVERLAY, JSON_MAP, JSON_LIST, COMPLETE_PDF, COMPLETE_PDF_STDIN} command;

#define DEFAULT_WIDTH 140
#define DEFAULT_HEIGHT 14
#define CP_BUFSIZE 32768
#define DEFAULT_SIG_VISIBLITY 1

#define UTF8_FIELD_NAME(ctx, obj) pdf_to_utf8(ctx, pdf_dict_get(ctx, obj, PDF_NAME_T));

#define RETURN_FILL_ERROR(fillenv, err) { fillenv->err_msg = err; return FILL_DATA_INVALID; }

typedef struct {
    float x;
    float y;
    float w;
    float h;
    const char *file;
    const char *password;
    int visible;
} signature_data;


typedef struct {
    float x;
    float y;
    float w;
    float h;
    int editable;
} text_data;


typedef struct {
    json_t *json_map_item;
    json_t *json_input_data;

    const char *input_key;
    const char *input_data;
    const char *err_msg;

    union {
        int field_id;
        const char *field_name;
        text_data text;
        signature_data sig;
    };
} fill_env;


typedef struct {
  fz_context *ctx;
  pdf_document *doc;
  command cmd;

  pdf_page *page;
  int page_num;
  int page_count;

  json_t *root;

  char *optFile;
  char *dataFile;
  char *inputPdf;
  char *outputPdf;
  char *sigFile;
  char *sigPwd;

} pdf_env;


typedef struct  {
    json_t *json_root;
    json_t *json_item;
} visit_env;


typedef void (*pre_visit_doc_func)(pdf_env *, visit_env *);
typedef void (*pre_visit_page_func)(pdf_env *, visit_env *);
typedef void (*visit_field_func)(pdf_env *, visit_env *, pdf_widget *w, int widget_num);
typedef void (*post_visit_page_func)(pdf_env *, visit_env *);
typedef void (*post_visit_doc_func)(pdf_env *, visit_env *);


typedef struct {
    pre_visit_doc_func pre_visit_doc;
    pre_visit_page_func pre_visit_page;
    visit_field_func visit_field;
    pre_visit_page_func post_visit_page;
    pre_visit_doc_func post_visit_doc;
} visit_funcs;


void cmplt_set_field_readonly(fz_context *ctx, pdf_document *doc, pdf_obj *field);
int cmplt_fcopy(const char *src, const char *dest);
int cmplt_add_textfield(pdf_env *env, fill_env *fillenv);
int cmplt_add_signature(pdf_env *env, fill_env *fillenv);
pdf_widget *cmplt_find_widget_name(fz_context *ctx, pdf_page *page, const char *field_name);
pdf_widget *cmplt_find_widget_id(fz_context *ctx, pdf_page *page, int field_id);
int cmplt_set_widget_value(pdf_env *env, pdf_widget *widget, const char *data);


int cmplt_fill_field(pdf_env *env, fill_env *fillenv);
void cmplt_fill_all(pdf_env *env, FILE *data_input);
pdf_widget *cmplt_find_widget_id(fz_context *ctx, pdf_page *page, int field_id);


fill_type fill_tpl_signature_data(pdf_env *env, fill_env *fillenv);
fill_type fill_tpl_text_data(pdf_env *env, fill_env *fillenv, fill_type success_type);
fill_type fill_tpl_data(pdf_env *env, fill_env *fillenv);


void parse_fields_doc(pdf_env *env);
void parse_fields_page(pdf_env *env, int page_num);
int parse_args(int argc, char **argv, pdf_env *env);


void visit_doc_init_json(pdf_env *penv, visit_env *venv);
void visit_page_init_json(pdf_env *penv, visit_env *venv);
void visit_page_end_json(pdf_env *penv, visit_env *venv);
void visit_doc_end_json(pdf_env *penv, visit_env *venv);
void visit_field_jsonmap(pdf_env *penv, visit_env *venv, pdf_widget *widget, int widget_num);

json_t *visit_field_json_shared(fz_context *ctx, pdf_document *doc, pdf_widget *widget);

void visit_field_overlay(pdf_env *penv, visit_env *venv, pdf_widget *widget, int widget_num);
void visit_page_end_overlay(pdf_env *penv, visit_env *venv);
void visit_doc_end_overlay(pdf_env *penv, visit_env *venv);

#endif
