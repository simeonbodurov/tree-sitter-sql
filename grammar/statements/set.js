import { comma_list } from "../helpers.js";

export default {

  // Firebird SET TERM: self-terminating leaf token — consumes the full
  // "SET TERM ^ ;" or "SET TERM ; ^" including the trailing terminator char
  // so the program rule does not expect an additional ; or ^ after it.
  set_term: _ => /[Ss][Ee][Tt][ \t]+[Tt][Ee][Rr][Mm][ \t]+\S+[ \t]*[;^]/,

  // Firebird DECLARE EXTERNAL FUNCTION: self-terminating leaf token — swallows the
  // entire declaration (name + CSTRING/INTEGER params + ENTRY_POINT + MODULE_NAME)
  // including the trailing semicolon. Using a regex avoids type-system conflicts
  // with CSTRING, BY VALUE, NULL, RETURNS, etc. which are not in the SQL grammar.
  declare_external_function: _ => /DECLARE[ \t\n\r]+EXTERNAL[ \t\n\r]+FUNCTION[^;]+;/i,

  // Firebird local variable declaration: appears before BEGIN in procedure/function bodies.
  // Covers both "DECLARE VARIABLE name type;" and "DECLARE name type;" forms.
  // The VARIABLE keyword is optional (older Firebird/legacy style omits it).
  // Only used inside procedure/function body context, not at top level.
  fb_var_declaration: _ => /DECLARE[ \t]+(?:VARIABLE[ \t]+)?\w+[ \t]+[^;]+;/i,

  // Firebird CREATE PROCEDURE/TRIGGER/FUNCTION in caret mode: self-terminating.
  // Absorbs the entire definition from CREATE ... through END^, where ^ is the
  // Firebird caret terminator (always at end of line, never inside the body).
  // Pattern: "^ not at end of line" = XOR or other use (allowed inside body);
  //          "^ at end of line or EOF" = statement terminator (stops the match).
  // This prevents the complex procedure body (Firebird variable assignments,
  // IF/WHILE, EXECUTE STATEMENT, triple-quote strings) from corrupting the
  // GLR parser state and causing subsequent CREATE TABLE nodes to be missed.
  fb_proc_or_trigger: _ => /CREATE[ \t\r\n]+(?:OR[ \t\r\n]+REPLACE[ \t\r\n]+)?(?:PROCEDURE|TRIGGER|FUNCTION)(?:[^\^]|\^[^\r\n])*\^/i,

  set_statement: $ => seq(
    $.keyword_set,
    choice(
      seq(
        optional(choice($.keyword_session, $.keyword_local)),
        choice(
          seq(
            $.object_reference,
            choice($.keyword_to, '='),
            choice(
              $.literal,
              $.keyword_default,
              $.identifier,
              $.keyword_on,
              $.keyword_off,
            ),
          ),
          seq($.keyword_schema, $.literal),
          seq($.keyword_names, $.literal),
          seq($.keyword_time, $.keyword_zone, choice($.literal, $.keyword_local, $.keyword_default)),
          seq($.keyword_session, $.keyword_authorization, choice($.identifier, $.keyword_default)),
          seq($.keyword_role, choice($.identifier, $.keyword_none)),
        ),
      ),
      seq($.keyword_constraints, choice($.keyword_all, comma_list($.identifier, true)), choice($.keyword_deferred, $.keyword_immediate)),
      seq($.keyword_transaction, $._transaction_mode),
      seq($.keyword_transaction, $.keyword_snapshot, $._transaction_mode),
      seq($.keyword_session, $.keyword_characteristics, $.keyword_as, $.keyword_transaction, $._transaction_mode),
    ),
  ),

  _transaction_mode: $ => seq(
    $.keyword_isolation,
    $.keyword_level,
    choice(
      $.keyword_serializable,
      seq($.keyword_repeatable, $.keyword_read),
      seq($.keyword_read, $.keyword_committed),
      seq($.keyword_read, $.keyword_uncommitted),
    ),
    choice(
      seq($.keyword_read, $.keyword_write),
      seq($.keyword_read, $.keyword_only),
    ),
    optional($.keyword_not),
    $.keyword_deferrable,
  ),

  reset_statement: $ => seq(
    $.keyword_reset,
    choice(
      $.object_reference,
      $.keyword_all,
      seq($.keyword_session, $.keyword_authorization),
      $.keyword_role,
    ),
  ),

};
