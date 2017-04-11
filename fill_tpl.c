#include "fill.h"

double fill_tpl_get_number(json_t *jsn_obj, const char *property, float default_val, float min_val) {
    json_t *jsn_val = json_object_get(jsn_obj, property);

    if(json_is_number(jsn_val) && json_number_value(jsn_val) >= min_val) {
        return json_number_value(jsn_val);
    } else {
       return default_val;
    }
}

void fill_tpl_get_position_data(json_t *jsn_obj, pos_data *pos, float default_xy, float default_width, float default_height) {
    pos->left = fill_tpl_get_number(jsn_obj, "left", default_xy, 0);
    pos->top = fill_tpl_get_number(jsn_obj, "top", default_xy, 0);
    pos->right = pos->left + fill_tpl_get_number(jsn_obj, "width", default_width, 1);
    pos->bottom = pos->top + fill_tpl_get_number(jsn_obj, "height", default_height, 1);
}


fill_type fill_tpl_signature_data(pdf_env *env, fill_env *fillenv) {
    fill_tpl_get_position_data(json_object_get(fillenv->json_map_item, "rect"), &fillenv->sig.pos, 0, DEFAULT_SIG_WIDTH, DEFAULT_SIG_HEIGHT);

    json_t *json_font = json_object_get(fillenv->json_map_item, "font");
    if(json_is_string(json_font)) {
        fillenv->sig.font = json_string_value(json_font);
        fillenv->sig.visible = 1;
    } else {
        fillenv->sig.font = 0;
        fillenv->sig.visible = 0;
    }

    json_t *json_sigfile = json_object_get(fillenv->json_map_item, "sigfile");

    if(!json_is_string(json_sigfile) && !env->sigFile) {
        RETURN_FILL_ERROR(fillenv, "Sigfile not set");
    }

    const char *sigfile = env->sigFile ? env->sigFile : json_string_value(json_sigfile);

    struct stat buffer;
    if(stat(sigfile, &buffer) != 0) {
        RETURN_FILL_ERROR(fillenv, "Sigfile not found");
    } else {
        fillenv->sig.file = sigfile;
    }

    json_t *json_pwd = json_object_get(fillenv->json_map_item, "password");

    if(env->sigPwd) {
        fillenv->sig.password = env->sigPwd;
    } else if(json_is_string(json_pwd)) {
        fillenv->sig.password = json_string_value(json_pwd);
    } else {
        RETURN_FILL_ERROR(fillenv, "Password not supplied");
    }

    return ADD_SIGNATURE;
}


fill_type fill_tpl_text_data(pdf_env *env, fill_env *fillenv, fill_type success_type) {
    fill_tpl_get_position_data(json_object_get(fillenv->json_map_item, "rect"), &fillenv->text.pos, -1, DEFAULT_TEXT_WIDTH, DEFAULT_TEXT_HEIGHT);

    if(fillenv->text.pos.left < 0 || fillenv->text.pos.top < 0) {
        RETURN_FILL_ERROR(fillenv, "Position invalid");
    }

    json_t *json_edit = json_object_get(fillenv->json_map_item, "editable");
    if(json_is_boolean(json_edit)) {
        fillenv->text.editable = json_boolean_value(json_edit);
    } else {
        fillenv->text.editable = 0;
    }

    json_t *json_font = json_object_get(fillenv->json_map_item, "font");
    if(json_is_string(json_font)) {
        fillenv->text.font = json_string_value(json_font);
    } else {
        fillenv->text.font = 0;
    }

    json_t *json_color= json_object_get(fillenv->json_map_item, "color");
    if(json_is_array(json_color) && json_array_size(json_color) == 3) {
        fillenv->text.color[0] = json_number_value(json_array_get(json_color, 0));
        fillenv->text.color[1] = json_number_value(json_array_get(json_color, 1));
        fillenv->text.color[2] = json_number_value(json_array_get(json_color, 2));
    } else {
        fillenv->text.color[0] = 0;
        fillenv->text.color[1] = 0;
        fillenv->text.color[2] = 0;
    }

    return success_type;
}


fill_type fill_tpl_data(pdf_env *env, fill_env *fillenv) {
    json_t *json_id = json_object_get(fillenv->json_map_item, "id");
    if(json_id != NULL) {
        if (!json_is_integer(json_id)) {
            RETURN_FILL_ERROR(fillenv, "Invalid field id, must be an integer");
        }

        fillenv->field_id = json_integer_value(json_id);
        return FIELD_ID;
    }

    json_t *json_name = json_object_get(fillenv->json_map_item, "name");
    if(json_name != NULL) {
        if (!json_is_string(json_name)) {
            RETURN_FILL_ERROR(fillenv, "Invalid field name, must be a string");
        }

        fillenv->field_name = json_string_value(json_name);
        return FIELD_NAME;
    }

    json_t *json_addtype = json_object_get(fillenv->json_map_item, "add");

    if(json_addtype == NULL) {
        RETURN_FILL_ERROR(fillenv, "Unrecognised template object. Needs an 'id', 'name' or 'add' property.");
    }

    const char *type_name = json_string_value(json_addtype);

    if(strncmp(type_name, "textfield", 13) == 0) {
        return fill_tpl_text_data(env, fillenv, ADD_TEXTFIELD);
    } else if(strncmp(type_name, "signature", 9) == 0) {
        return fill_tpl_signature_data(env, fillenv);
    } else {
        RETURN_FILL_ERROR(fillenv, "Trying to add an unrecognised type. Only 'textfield' and 'signature' supported.");
    }

}
