#ifndef PDF_FILL_H
#define PDF_FILL_H

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <jansson.h>

typedef enum {FILL_DATA_INVALID, FIELD_ID, FIELD_NAME, ADD_TEXTFIELD, ADD_SIGNATURE, ADD_IMAGE} fill_type;

typedef enum {ANNOTATE_FIELDS, JSON_LIST, JSON_MAP, FONT_LIST, COMPLETE_PDF} command;

#define CMD_COUNT 5

const char *command_names[CMD_COUNT];
extern fz_document_handler pdf_document_handler;


#define DEFAULT_SIG_WIDTH 30
#define DEFAULT_SIG_HEIGHT 30

#define DEFAULT_TEXT_WIDTH 140
#define DEFAULT_TEXT_HEIGHT 14
#define DEFAULT_FONT_HEIGHT 9

#define CP_BUFSIZE 32768
#define DEFAULT_SIG_VISIBLITY 1
#define MAX_ERRLEN 160

#define UTF8_FIELD_NAME(ctx, obj) pdf_to_utf8(ctx, pdf_dict_get(ctx, obj, PDF_NAME_T));

#define RETURN_FILL_ERROR(err) { \
    fprintf(stderr, err); \
    return FILL_DATA_INVALID; \
}

#define RETURN_FILL_ERROR_ARG(err, arg) { \
    fprintf(stderr, err, arg); \
    return FILL_DATA_INVALID; \
}

#define INIT_CAP 8


// vg = vector graphics. a simple wrapper of mupdf's internal vg drawing api

typedef enum { VG_STROKE, VG_FILL } vg_path_type;
typedef enum { VG_MOVE, VG_LINE, VG_HORIZ, VG_VERT, VG_CURVE, VG_CLOSE } vg_cmd_type;

typedef struct _vg_coord {
    float val;
} vg_coord;

typedef struct _vg_pt {
    float x, y;
} vg_pt;

typedef struct _vg_curve {
    vg_pt pt1, pt2, pt3;
} vg_curve;

//typedef struct _vg_cmd_data {
//    union {
//        vg_coord p;
//        vg_pt pt;
//        vg_curve curve;
//    };
//} vg_cmd_data;

typedef struct _vg_cmd {
    vg_cmd_type type;

    union {
        vg_pt pt;
        vg_curve curve;
        float data;
    };
} vg_cmd;


typedef struct _vg_list {
    int len;
    int cap;
    int sizeof_item;
    void **items;
} vg_list;

typedef struct _vg_stroke {

} vg_stroke;

typedef struct _vg_path {
    vg_path_type type;
    union {
        float rgba[4];
        struct {
            float r,g,b,a;
        };
    };
    vg_list *cmds;

    union {
        fz_stroke_state stroke;
    };

} vg_path;

typedef vg_list vg_pathlist;


typedef struct _vg_fz_pathlist {
    fz_context *ctx;
    vg_list *paths;
} vg_fz_pathlist;

// these structures hold data from the json template file

typedef struct {
    float left;
    float top;
    float right;
    float bottom;
} pos_data;


typedef struct {
    pos_data pos;
    const char *widget_name;
    const char *font;
    const char *file;
    const char *password;
    int visible;
    int page_num;
} signature_data;


typedef struct {
    pos_data pos;
    const char *file_name;
} image_data;


typedef struct {
    pos_data pos;
    int editable;
    const char *font;
    float color[3];
} text_data;


typedef struct {
    char *input;
    char *output;
} files_env;

// used by in complete.c for the cli complete command

typedef struct {
    files_env files;
    char *dataFile;
    char *tplFile;
    char *certFile;
    char *certPwd;

    json_t *json_map_item;
    json_t *json_input_data;

    const char *input_key;
    const char *input_data;

    union {
        int field_id;
        const char *field_name;
        text_data text;
        signature_data sig;
        image_data img;
    };
} _fill_env;


// used in visit.c for info parser cmds
typedef struct  {
    files_env files;
    json_t *json_root;
    json_t *json_item;
} _visit_env;



typedef struct {
  fz_context *ctx;
  pdf_document *doc;
  command cmd;

  pdf_page *page;
  int page_num;
  int page_count;

  json_t *root;

  union {
    _fill_env fill;
    _visit_env parse;
    files_env files;
  };

  int add_sig;
  signature_data add_sig_data;
} pdf_env;


// used by info parsers. one parser get widget info, the other font info, the other general pdf info.
// the pdf page parsing method is the same for each so parsed different output funcs
typedef void (*pre_visit_doc_func)(pdf_env *);
typedef void (*pre_visit_page_func)(pdf_env *);
typedef void (*visit_widget_func)(pdf_env *, pdf_widget *w, int widget_num);
typedef void (*post_visit_page_func)(pdf_env *);
typedef void (*post_visit_doc_func)(pdf_env *);

typedef struct {
    pre_visit_doc_func pre_visit_doc;
    pre_visit_page_func pre_visit_page;
    visit_widget_func visit_widget;
    pre_visit_page_func post_visit_page;
    pre_visit_doc_func post_visit_doc;
} visit_funcs;

//fill.c
void usage_message(int cmd);
int read_parse_cmd_args(int argc, char **argv, pdf_env *env);
int read_completion_cmd_args(int argc, char **argv, pdf_env *env);
command get_command(char *cmd);
const char *command_name(command cmd);
int read_args(int argc, char **argv, pdf_env *env);
int main(int argc, char **argv);


//complete.c
int cmplt_fill_field(pdf_env *env);
void cmplt_fill_all(pdf_env *env);
int cmplt_da_str(const char *font, float *color, char *buf);
void cmplt_set_field_readonly(fz_context *ctx, pdf_document *doc, pdf_obj *field);
int cmplt_fcopy(const char *src, const char *dest);
static int cmplt_sign_and_save(pdf_env *env);

int cmplt_add_image(pdf_env *env);
int cmplt_add_signature(fz_context *ctx, pdf_document *doc, pdf_page *page, signature_data *sig);
int cmplt_add_textfield(pdf_env *env);
int cmplt_set_widget_value(pdf_env *env, pdf_widget *widget, const char *data);
pdf_widget *cmplt_find_widget_name(fz_context *ctx, pdf_page *page, const char *field_name);
pdf_widget *cmplt_find_widget_id(fz_context *ctx, pdf_page *page, int field_id);
int str_is_all_digits(const char *str);
fz_buffer *cmplt_deflatebuf(fz_context *ctx, unsigned char *p, size_t n);


//map_input.c
double map_input_number(json_t *jsn_obj, const char *property, float default_val, float min_val);
void map_input_posdata(json_t *jsn_obj, pos_data *pos, float default_xy, float default_width, float default_height);
fill_type map_input_signature(pdf_env *env);
fill_type map_input_textfield(pdf_env *env, fill_type success_type);
fill_type map_input_data(pdf_env *env);
fill_type map_input_image(pdf_env *env);

//parse.c
visit_funcs get_visitor_funcs(int cmd);
void parse_fields_doc(pdf_env *env);
void parse_fields_page(pdf_env *env, int page_num);

void visit_page_fontlist(pdf_env *penv);
void visit_doc_end_fontlist(pdf_env *penv);
void visit_widget_fontlist(pdf_env *env, pdf_widget *widget, int widget_num);

void visit_doc_init_json(pdf_env *penv);
void visit_page_init_json(pdf_env *penv);
void visit_page_end_json(pdf_env *penv);
void visit_doc_end_json(pdf_env *penv);
void visit_widget_jsonmap(pdf_env *penv, pdf_widget *widget, int widget_num);
void visit_field_jsonlist(pdf_env *env, pdf_widget *widget, int widget_num);
json_t *visit_field_json_shared(fz_context *ctx, pdf_document *doc, pdf_widget *widget);

void visit_widget_overlay(pdf_env *penv, pdf_widget *widget, int widget_num);
void visit_page_end_overlay(pdf_env *penv);
void visit_doc_end_overlay(pdf_env *penv);

//util.c

pdf_obj *u_pdf_add_image(fz_context *ctx, pdf_document *doc, fz_image *image, int mask);
pdf_obj *u_pdf_find_image_resource(fz_context *ctx, pdf_document *doc, fz_image *item, unsigned char digest[16]);
void u_pdf_preload_image_resources(fz_context *ctx, pdf_document *doc);
void u_fz_md5_image(fz_context *ctx, fz_image *image, unsigned char digest[16]);
void u_pdf_sign_signature(fz_context *ctx, pdf_document *doc, pdf_widget *widget, const char *sigfile, const char *password, vg_pathlist *pathlist, const char *overlay_msg);
void u_pdf_set_signature_appearance(fz_context *ctx, pdf_document *doc, pdf_annot *annot, vg_pathlist *pathlist, const char *msg_1);

//vg_path.c

vg_path *vg_new_path(vg_path_type type, float rgba[4]);
vg_cmd *vg_add_cmd(vg_path *path, vg_cmd *cmd);

vg_cmd *vg_horizto(float);
vg_cmd *vg_vertto(float);
vg_cmd *vg_moveto(float, float);
vg_cmd *vg_lineto(float, float);
vg_cmd *vg_curveto(float, float, float, float, float, float);
vg_cmd *vg_close();


vg_cmd *vg_get_cmd(vg_path *path, int idx);
void vg_free_path(vg_path *path);

vg_pathlist *vg_new_pathlist();
vg_path *vg_add_path(vg_pathlist *pathlist, vg_path *path);
vg_path *vg_get_path(vg_pathlist *pathlist, int idx);
void vg_free_pathlist(vg_pathlist *pathlist);

void vg_drop_fz_paths(vg_fz_pathlist *);

vg_fz_pathlist *vg_draw_pathlist(fz_context *ctx, fz_device *dev, fz_rect *rect, fz_matrix *page_ctm, vg_pathlist *pathlist);


#endif
