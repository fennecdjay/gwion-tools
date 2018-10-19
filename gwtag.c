#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "absyn.h"
#include "hash.h"
#include "scanner.h"

#define TABLEN 2

extern m_str op2str(Operator);

typedef struct {
  const m_str  filename;
  Vector class_stack;
  FILE*  file;
} Tagger;

static void tag_exp(Tagger* tagger, Exp exp);
static void tag_type_decl(Tagger* tagger, Type_Decl* type);
static void tag_stmt(Tagger* tagger, Stmt stmt);
static void tag_stmt_list(Tagger* tagger, Stmt_List list);
static void tag_class_def(Tagger* tagger, Class_Def class_def);

static void tag_print(Tagger* tagger, const char* fmt, ...) {
  va_list arg;
  va_start(arg, fmt);
  vfprintf(tagger->file, fmt, arg);
  va_end(arg);
}

static void tag(Tagger* tagger, const m_str name) {
  tag_print(tagger, "%s\t%s\t", name, tagger->filename);
}

static void tag_id_list(Tagger* tagger, ID_List list) {
  m_bool next = list->next ? 1 : 0;
  if(next)
    tag_print(tagger, "<");
  while(list) {
    tag_print(tagger, s_name(list->xid));
    list = list->next;
    if(list)
      tag_print(tagger, ".");
  }
  if(next)
    tag_print(tagger, ">");
}

static void tag_array(Tagger* tagger, Array_Sub array) {
  m_uint i;
  Exp exp = array->exp;
  for(i = 0; i < array->depth; i++) {
    Exp tmp = exp ? exp->next : NULL;
    if(exp)
      exp->next = NULL;
    tag_print(tagger, "[");
    if(exp)
     tag_exp(tagger, exp);
    tag_print(tagger, "]");
    if(exp) {
      exp->next = tmp;
      exp = tmp;
    }
  }
}

static void tag_type_list(Tagger* tagger, Type_List list) {
  tag_print(tagger, "<{");
  while(list) {
    tag_type_decl(tagger, list->td);
    list = list->next;
    if(list)
      tag_print(tagger, ", ");
  }
  tag_print(tagger, "}>");
}

static void tag_type_decl(Tagger* tagger, Type_Decl* type) {
  if(type->xid->ref) {
    tag_print(tagger, "typeof ");
    tag_id_list(tagger, type->xid->ref ? type->xid->ref : type->xid);
  } else {
    if(type->types)
      tag_type_list(tagger, type->types);
    tag_id_list(tagger, type->xid);
  }
  if(GET_FLAG(type, ae_flag_ref))
    tag_print(tagger, "@");
  if(type->array)
    tag_array(tagger, type->array);
}

static void tag_exp_decl(Tagger* tagger, Exp_Decl* decl) {
  Var_Decl_List list = decl->list;
  while(list) {
    tag(tagger, s_name(list->self->xid));
    tag_print(tagger, "/^");
    tag_type_decl(tagger, decl->td);
    if(list->self->array)
      tag_array(tagger, list->self->array);
    tag_print(tagger, "$/;\"\t%s\n", vector_front(tagger->class_stack) ?
        "m" : "v");
    list = list->next;
  }
}

static void tag_exp_unary(Tagger* tagger __attribute__((unused)), Exp_Unary* unary __attribute__((unused))) {
  return;
}

static void tag_exp_binary(Tagger* tagger, Exp_Binary* binary) {
  tag_exp(tagger, binary->lhs);
  tag_exp(tagger, binary->rhs);
}

static void tag_exp_primary(Tagger* tagger __attribute__((unused)), Exp_Primary* exp __attribute__((unused))) {
  return;
}

static void tag_exp_array(Tagger* tagger, Exp_Array* array) {
  tag_exp(tagger, array->base);
  tag_exp(tagger, array->array->exp);
}

static void tag_exp_cast(Tagger* tagger, Exp_Cast* cast) {
  tag_exp(tagger, cast->exp);
}

static void tag_exp_post(Tagger* tagger, Exp_Postfix* post) {
  tag_exp(tagger, post->exp);
}

static void tag_exp_call(Tagger* tagger __attribute__((unused)), Exp_Call* exp_call __attribute__((unused))) {
  return;
}

static void  tag_exp_dot(Tagger* tagger __attribute__((unused)), Exp_Dot* member __attribute__((unused))) {
  return;
}

static void  tag_exp_dur(Tagger* tagger __attribute__((unused)), Exp_Dur* member __attribute__((unused))) {
  return;
}

static void tag_exp_if(Tagger* tagger, Exp_If* exp_if) {
  tag_exp(tagger, exp_if->cond);
  tag_exp(tagger, exp_if->if_exp);
  tag_exp(tagger, exp_if->else_exp);
}

static void tag_exp(Tagger* tagger,  Exp exp) {
  while(exp) {
    switch(exp->exp_type) {
      case ae_exp_primary:
        tag_exp_primary(tagger, &exp->d.exp_primary);
        break;
      case ae_exp_decl:
        tag_exp_decl(tagger, &exp->d.exp_decl);
        break;
      case ae_exp_unary:
        tag_exp_unary(tagger, &exp->d.exp_unary);
        break;
      case ae_exp_binary:
        tag_exp_binary(tagger, &exp->d.exp_binary);
        break;
      case ae_exp_post:
        tag_exp_post(tagger, &exp->d.exp_post);
        break;
      case ae_exp_cast:
        tag_exp_cast(tagger, &exp->d.exp_cast);
        break;
      case ae_exp_call:
        tag_exp_call(tagger, &exp->d.exp_call);
        break;
      case ae_exp_array:
        tag_exp_array(tagger, &exp->d.exp_array);
        break;
      case ae_exp_dot:
        tag_exp_dot(tagger, &exp->d.exp_dot);
        break;
      case ae_exp_dur:
        tag_exp_dur(tagger, &exp->d.exp_dur);
        break;
      case ae_exp_if:
        tag_exp_if(tagger, &exp->d.exp_if);
        break;
      default:
        break;
    }
    exp = exp->next;
  }
}

static void tag_stmt_code(Tagger* tagger, Stmt_Code stmt) {
  if(stmt->stmt_list)
    tag_stmt_list(tagger, stmt->stmt_list);
}

static void tag_stmt_return(Tagger* tagger, Stmt_Exp stmt) {
  if(stmt->val)
    tag_exp(tagger, stmt->val);
}

static void tag_stmt_flow(Tagger* tagger, struct Stmt_Flow_* stmt) {
  tag_exp(tagger, stmt->cond);
  tag_stmt(tagger, stmt->body);
}

static void tag_stmt_for(Tagger* tagger, Stmt_For stmt) {
  tag_stmt(tagger, stmt->c1);
  tag_stmt(tagger, stmt->c2);
  tag_exp(tagger, stmt->c3);
  tag_stmt(tagger, stmt->body);
}

static void tag_stmt_auto(Tagger* tagger, Stmt_Auto stmt) {
  // TODO tag id
  tag_exp(tagger, stmt->exp);
  tag_stmt(tagger, stmt->body);
}
static void tag_stmt_loop(Tagger* tagger, Stmt_Loop stmt) {
  tag_exp(tagger, stmt->cond);
  tag_stmt(tagger, stmt->body);
}

static void tag_stmt_switch(Tagger* tagger, Stmt_Switch stmt) {
  tag_exp(tagger, stmt->val);
}

static void tag_stmt_case(Tagger* tagger, Stmt_Exp stmt) {
  tag_exp(tagger, stmt->val);
}

static void tag_stmt_if(Tagger* tagger, Stmt_If stmt) {
  tag_exp(tagger, stmt->cond);
  tag_stmt(tagger, stmt->if_body);
  if(stmt->else_body)
    tag_stmt(tagger, stmt->else_body);
}

void tag_stmt_enum(Tagger* tagger, Stmt_Enum stmt) {
  ID_List list = stmt->list;
  if(stmt->xid) {
    tag(tagger, s_name(stmt->xid));
    tag_print(tagger, "0;\"\tt");
  }
  while(list) {
    tag(tagger, s_name(list->xid));
    tag_print(tagger, "0;\"\te");
    list = list->next;
  }
}

void tag_stmt_fptr(Tagger* tagger, Stmt_Fptr ptr) {
  Arg_List list = ptr->args;
  tag(tagger, s_name(ptr->xid));
  tag_print(tagger, "/^");
  tag_type_decl(tagger, ptr->td);
  tag_print(tagger, " ");
  tag_print(tagger, s_name(ptr->xid));
  tag_print(tagger, "(");
  while(list) {
    tag_type_decl(tagger, list->td);
    tag_print(tagger, " %s", s_name(list->var_decl->xid));
    list = list->next;
    if(list)
      tag_print(tagger, ", ");
    else if(GET_FLAG(ptr->td, ae_flag_variadic))
      tag_print(tagger, ", ...");
  }

  tag_print(tagger, ") {$/;\tt\n");
}

void tag_stmt_type(Tagger* tagger, Stmt_Type ptr) {
  tag(tagger, s_name(ptr->xid));
  tag_print(tagger, "/^");
  tag_type_decl(tagger, ptr->td);
  tag_print(tagger, " ");
  tag_print(tagger, s_name(ptr->xid));
  tag_print(tagger, ") {$/;\tt\n");
}

void tag_stmt_union(Tagger* tagger, Stmt_Union stmt) {
  Decl_List l = stmt->l;
  if(stmt->xid) {
    tag(tagger, s_name(stmt->xid));
    tag_print(tagger, "0;\"\tu");
  }
  while(l) {
    tag_exp(tagger, l->self);
    l = l->next;
  }
}

void tag_stmt_goto(Tagger* tagger __attribute__((unused)), Stmt_Jump stmt __attribute__((unused))) {
  return;
}

void tag_stmt_continue(Tagger* tagger __attribute__((unused)), Stmt stmt __attribute__((unused))) {
  return;
}

void tag_stmt_break(Tagger* tagger __attribute__((unused)), Stmt stmt __attribute__((unused))) {
  return;
}

static void tag_stmt(Tagger* tagger, Stmt stmt) {
  if(stmt->stmt_type == ae_stmt_exp && !stmt->d.stmt_exp.val)
    return;
  switch(stmt->stmt_type) {
    case ae_stmt_exp:
      tag_exp(tagger, stmt->d.stmt_exp.val);
      break;
    case ae_stmt_code:
      tag_stmt_code(tagger, &stmt->d.stmt_code);
      break;
    case ae_stmt_return:
      tag_stmt_return(tagger, &stmt->d.stmt_exp);
      break;
    case ae_stmt_if:
      tag_stmt_if(tagger, &stmt->d.stmt_if);
      break;
    case ae_stmt_while:
      tag_stmt_flow(tagger, &stmt->d.stmt_flow);
      break;
    case ae_stmt_for:
      tag_stmt_for(tagger, &stmt->d.stmt_for);
      break;
    case ae_stmt_auto:
      tag_stmt_auto(tagger, &stmt->d.stmt_auto);
      break;
    case ae_stmt_until:
      tag_stmt_flow(tagger, &stmt->d.stmt_flow);
      break;
    case ae_stmt_loop:
      tag_stmt_loop(tagger, &stmt->d.stmt_loop);
      break;
    case ae_stmt_switch:
      tag_stmt_switch(tagger, &stmt->d.stmt_switch);
      break;
    case ae_stmt_case:
      tag_stmt_case(tagger, &stmt->d.stmt_exp);
      break;
    case ae_stmt_enum:
      tag_stmt_enum(tagger, &stmt->d.stmt_enum);
      break;
    case ae_stmt_continue:
      tag_stmt_continue(tagger, stmt);
      break;
    case ae_stmt_break:
      tag_stmt_break(tagger, stmt);
      break;
    case ae_stmt_jump:
      tag_stmt_goto(tagger, &stmt->d.stmt_jump);
      break;
    case ae_stmt_fptr:
      tag_stmt_fptr(tagger, &stmt->d.stmt_fptr);
      break;
    case ae_stmt_type:
      tag_stmt_type(tagger, &stmt->d.stmt_type);
      break;
    case ae_stmt_union:
      tag_stmt_union(tagger, &stmt->d.stmt_union);
      break;
    default:break;
  }
}

static void tag_stmt_list(Tagger* tagger, Stmt_List list) {
  while(list) {
    tag_stmt(tagger, list->stmt);
    list = list->next;
  }
}

static void tag_func_def(Tagger* tagger, Func_Def f) {
  Arg_List list = f->arg_list;
  tag_print(tagger, s_name(f->name));
  tag_print(tagger, "/^");
  if(f->tmpl && f->tmpl->base) {
    tag_print(tagger, "template ");
    tag_id_list(tagger, f->tmpl->list);
    tag_print(tagger, " ");
  }
  tag_type_decl(tagger, f->td);
  tag_print(tagger, " ");
  tag_print(tagger, s_name(f->name));
  tag_print(tagger, "(");
  while(list) {
    tag_type_decl(tagger, list->td);
    tag_print(tagger, " %s", s_name(list->var_decl->xid));
    list = list->next;
    if(list)
      tag_print(tagger, ", ");
  }
  if(GET_FLAG(f, ae_flag_variadic))
    tag_print(tagger, ", ...");
  tag_print(tagger, ") {$/;\tf\n");
  f->flag &= ~ae_flag_template;
}

static void tag_section(Tagger* tagger, Section* section) {
  ae_section_t t = section->section_type;
  if(t == ae_section_stmt)
    tag_stmt_list(tagger, section->d.stmt_list);
  else if(t == ae_section_func)
    tag_func_def(tagger, section->d.func_def);
  else if(t == ae_section_class)
    tag_class_def(tagger, section->d.class_def);
}

static void tag_class_def(Tagger* tagger, Class_Def class_def) {
  Class_Body body = class_def->body;
  tag(tagger, s_name(class_def->name->xid));
  tag_print(tagger, "/^");
  if(class_def->tmpl) {
    tag_print(tagger, "template");
    tag_id_list(tagger, class_def->tmpl->list.list);
  }
// TODO: handle class extend
  tag_print(tagger, " class %s$/;\"\tc\n", s_name(class_def->name->xid));
  vector_add(tagger->class_stack, (vtype)class_def);
  while(body) {
    tag_section(tagger, body->section);
    body = body->next;
  }
  vector_pop(tagger->class_stack);
}

void tag_ast(Tagger* tagger, Ast ast) {
  while(ast) {
    tag_section(tagger, ast->section);
    ast = ast->next;
  }
}

int main(int argc, char** argv) {
  argc--; argv++;
  Scanner* scan = new_scanner(127); // magic number
  while(argc--) {
    Ast ast;
    Tagger tagger = { *argv , new_vector(), NULL };
    char c[strlen(*argv) + 6];
    sprintf(c, "%s.tag", *argv);
    FILE* f = fopen(*argv, "r");
    if(!f)continue;
    if(!(ast = parse(scan, *argv++, f))) {
      fclose(f);
      continue;
    }
    tagger.file = fopen(c, "w");
    tag_ast(&tagger, ast);
    free_ast(ast);
    free_vector(tagger.class_stack);
    fclose(tagger.file);
    fclose(f);
  }
  free_scanner(scan);
  free_symbols();
  return 0;
}
