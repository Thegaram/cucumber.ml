#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <wchar.h>
#include <string.h>

#include "file_reader.h"
#include "source_event.h"
#include "token_scanner.h"
#include "token_matcher.h"
#include "string_token_scanner.h"
#include "parser.h"
#include "ast_builder.h"
#include "gherkin_document_event.h"
#include "compiler.h"
#include "event.h"

#include "pickle_argument.h"
#include "pickle_string.h"
#include "pickle_event.h"
#include "pickle_table.h"
#include "pickle_row.h"
#include "pickle_cell.h"

#include "caml/mlvalues.h"
#include "caml/alloc.h"
#include "caml/memory.h"
#include "caml/fail.h"

char * char_of_wchar(const wchar_t *);
CAMLprim value create_ocaml_pickle(const Pickle *);
CAMLprim value create_ocaml_loc_list(const PickleLocations *);
CAMLprim value create_ocaml_tag_list(const PickleTags *);
CAMLprim value create_ocaml_loc(const PickleLocation *);
CAMLprim value create_ocaml_tag(const PickleTag *);
CAMLprim value create_ocaml_step_list(const PickleSteps *);
CAMLprim value create_ocaml_step(const PickleStep *);
CAMLprim value create_ocaml_docstring(const PickleArgument *);
CAMLprim value create_ocaml_table(const PickleArgument *);
CAMLprim value create_ocaml_table_row(const PickleRow *);
CAMLprim value create_ocaml_table_row_list(const PickleRows *);
CAMLprim value create_ocaml_table_cell(const PickleCell *);
CAMLprim value create_ocaml_error(const Error *);

CAMLprim value load_feature_file(value fileName) {
  setlocale(LC_ALL, "en_US.UTF-8");
  setbuf(stdout, NULL);
  
  CAMLparam1(fileName);
  CAMLlocal3(oPickleList, oPickle, cons);
  char *sFileName = String_val(fileName);
  int result_code = 0;
  
  FileReader *file_reader = FileReader_new(sFileName);
  SourceEvent *source_event = SourceEvent_new(sFileName, FileReader_read(file_reader));
  TokenScanner *token_scanner = StringTokenScanner_new(source_event->source);
  TokenMatcher* token_matcher = TokenMatcher_new(L"en");
  Builder* builder = AstBuilder_new();
  Parser* parser = Parser_new(builder);
  Compiler* compiler = Compiler_new();
  
  result_code = Parser_parse(parser, token_matcher, token_scanner);

  oPickleList = Val_emptylist;
  
  if(result_code == 0) {
    const GherkinDocumentEvent* gherkin_document_event = GherkinDocumentEvent_new(sFileName, AstBuilder_get_result(builder));

    result_code = Compiler_compile(compiler, gherkin_document_event->gherkin_document);

    Event_delete((const Event*)gherkin_document_event);

    if(result_code == 0) {
      while(Compiler_has_more_pickles(compiler)) {
	const PickleEvent* pickle_event = PickleEvent_new(sFileName, Compiler_next_pickle(compiler));
	const Pickle *pickle = pickle_event->pickle;

	oPickle = create_ocaml_pickle(pickle);

	
	cons = caml_alloc(2, 0);

	Store_field(cons, 0, oPickle);
	Store_field(cons, 1, oPickleList);

	oPickleList = cons;

	
	Event_delete((const Event*)pickle_event);
      }
    }
  } else {
    while(Parser_has_more_errors(parser)) {
      Error *error = Parser_next_error(parser);

      char *error_str = char_of_wchar(error->error_text);
      char *error_out = NULL;

      sprintf(error_out, "%s at line %i column %i", error_str, error->location.line, error->location.column);
      
      caml_failwith(error_out);
    }
  }
  
  FileReader_delete(file_reader);
  Event_delete((const Event*)source_event);
  Compiler_delete(compiler);
  Parser_delete(parser);
  AstBuilder_delete(builder);
  TokenMatcher_delete(token_matcher);
  
  CAMLreturn(oPickleList);
}

CAMLprim value create_ocaml_pickle(const Pickle *pickle) {
  CAMLparam0();
  CAMLlocal4(oPickle, oLocList, oTagList, oStepList);
  oPickle = caml_alloc(5, 0);

  char *lang = char_of_wchar(pickle->language);
  char *name = char_of_wchar(pickle->name);
  
  oLocList = create_ocaml_loc_list(pickle->locations);
  oTagList = create_ocaml_tag_list(pickle->tags);
  oStepList = create_ocaml_step_list(pickle->steps);
  
  Store_field(oPickle, 0, caml_copy_string(lang));
  Store_field(oPickle, 1, caml_copy_string(name));
  Store_field(oPickle, 2, oLocList);
  Store_field(oPickle, 3, oTagList);
  Store_field(oPickle, 4, oStepList);

  free(lang);
  free(name);
  
  CAMLreturn(oPickle);
}

CAMLprim value create_ocaml_loc_list(const PickleLocations *locs) {
  CAMLparam0();
  CAMLlocal3(oLocList, oLoc, cons);

  oLocList = Val_emptylist;

  if(locs == NULL) {
    CAMLreturn(oLocList);
  }
  
  for(int i = 0; i < locs->location_count; ++i) {
    cons = caml_alloc(2, 0);
    oLoc = create_ocaml_loc(&locs->locations[i]);
    
    Store_field(cons, 0, oLoc);
    Store_field(cons, 1, oLocList);

    oLocList = cons;
  }

  CAMLreturn(oLocList);
}

CAMLprim value create_ocaml_loc(const PickleLocation *loc) {
  CAMLparam0();
  CAMLlocal1(oLoc);
  
  oLoc = caml_alloc(2, 0);
  
  Store_field(oLoc, 0, caml_copy_int32(loc->line));
  Store_field(oLoc, 1, caml_copy_int32(loc->column));

  CAMLreturn(oLoc);
}

CAMLprim value create_ocaml_tag(const PickleTag *tag) {
  CAMLparam0();
  CAMLlocal1(oTag);

  oTag = caml_alloc(2, 0);

  char *name = char_of_wchar(tag->name);
  
  Store_field(oTag, 0, create_ocaml_loc(&tag->location));
  Store_field(oTag, 1, caml_copy_string(name));

  free(name);
  
  CAMLreturn(oTag);
}

CAMLprim value create_ocaml_tag_list(const PickleTags *tags) {
  CAMLparam0();
  CAMLlocal3(oTagList, oTag, cons);

  oTagList = Val_emptylist;

  if(tags == NULL) {
    CAMLreturn(oTagList);
  }
  
  for(int i = 0; i < tags->tag_count; ++i) {
    cons = caml_alloc(2, 0);
    oTag = create_ocaml_tag(&tags->tags[i]);
    Store_field(cons, 0, oTag);
    Store_field(cons, 1, oTagList);

    oTagList = cons;
  }

  CAMLreturn(oTagList);
}

CAMLprim value create_ocaml_step(const PickleStep *step) {
    CAMLparam0();
    CAMLlocal2(oStep, arg);

    oStep = caml_alloc(3, 0);

    Store_field(oStep, 0, create_ocaml_loc_list(step->locations));

    char *text = char_of_wchar(step->text);
    
    Store_field(oStep, 1, caml_copy_string(text));

    free(text);
    
    if(step->argument == NULL) {
      Store_field(oStep, 2, Val_int(0));
      CAMLreturn(oStep);
    }

    switch(step->argument->type) {
    case Argument_String:
      arg = caml_alloc(1, 0);
      Store_field(arg, 0, create_ocaml_docstring(step->argument));
      Store_field(oStep, 2, arg);
      break;
    case Argument_Table:
      arg = caml_alloc(1, 1);
      Store_field(arg, 0, create_ocaml_table(step->argument));
      Store_field(oStep, 2, arg);
      break;
    default:
      Store_field(oStep, 2, Val_int(0));
    }

    CAMLreturn(oStep);
}

CAMLprim value create_ocaml_step_list(const PickleSteps *steps) {
  CAMLparam0();
  CAMLlocal3(oStepList, oStep, cons);

  oStepList = Val_emptylist;

  if(steps == NULL) {
    CAMLreturn(oStepList);
  }
  
  for(int i = 0; i < steps->step_count; ++i) {
    cons = caml_alloc(2, 0);
    oStep = create_ocaml_step(&steps->steps[i]);
    Store_field(cons, 0, oStep);
    Store_field(cons, 1, oStepList);

    oStepList = cons;
  }

  CAMLreturn(oStepList);  
}

CAMLprim value create_ocaml_docstring(const PickleArgument *arg) {
  CAMLparam0();
  CAMLlocal1(oDocString);

  oDocString = caml_alloc(2, 0);

  const PickleString *docstring = (const PickleString*) arg;
  char *content = char_of_wchar(docstring->content);
  
  Store_field(oDocString, 0, create_ocaml_loc(&docstring->location));
  Store_field(oDocString, 1, caml_copy_string(content));

  free(content);

  CAMLreturn(oDocString);
}

CAMLprim value create_ocaml_table(const PickleArgument *arg) {
  CAMLparam0();
  CAMLlocal1(oTable);

  oTable = caml_alloc(1, 0);

  const PickleTable *table = (const PickleTable *) arg;
  
  Store_field(oTable, 0, create_ocaml_table_row_list(table->rows));
  
  CAMLreturn(oTable);
}

CAMLprim value create_ocaml_table_row_list(const PickleRows *rows) {
  CAMLparam0();
  CAMLlocal3(oRowList, cons, oRow);

  oRowList = Val_emptylist;

  for(int i = 0; i < rows->row_count; ++i) {
    cons = caml_alloc(2, 0);
    oRow = create_ocaml_table_row(&rows->pickle_rows[i]);

    Store_field(cons, 0, oRow);
    Store_field(cons, 1, oRowList);

    oRowList = cons;
  }

  CAMLreturn(oRowList);
}

CAMLprim value create_ocaml_table_row(const PickleRow *row) {
  CAMLparam0();
  CAMLlocal4(oCellList, oCell, oRow, cons);

  oCellList = Val_emptylist;
  
  for(int i = 0; i < row->pickle_cells->cell_count; ++i) {
    cons = caml_alloc(2, 0);
    oCell = create_ocaml_table_cell(&row->pickle_cells->pickle_cells[i]);

    Store_field(cons, 0, oCell);
    Store_field(cons, 1, oCellList);

    oCellList = cons;
  }

  oRow = caml_alloc(1, 0);

  Store_field(oRow, 0, oCellList);

  CAMLreturn(oRow);
}

CAMLprim value create_ocaml_table_cell(const PickleCell *cell) {
  CAMLparam0();
  CAMLlocal1(oCell);

  oCell = caml_alloc(2, 0);
  char *val = char_of_wchar(cell->value);
    
  Store_field(oCell, 0, create_ocaml_loc(cell->location));
  Store_field(oCell, 1, caml_copy_string(val));

  free(val);

  CAMLreturn(oCell);
}

char * char_of_wchar(const wchar_t *text) {
    size_t text_len = wcstombs(NULL, text, 0);
    char *text_out = malloc(sizeof(char) * (text_len + 1));
    wcstombs(text_out, text, text_len + 1);

    return text_out;
}
