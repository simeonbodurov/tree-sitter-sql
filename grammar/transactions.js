export default {

  transaction: $ => seq(
    $.keyword_begin,
    optional(
      $.keyword_transaction,
    ),
    optional(';'),
    repeat(
      seq(
        $.statement,
        ';'
      ),
    ),
    choice(
      $._commit,
      $._rollback,
    ),
  ),

  _commit: $ => seq(
    $.keyword_commit,
    optional(
      choice($.keyword_transaction, $.keyword_work),
    ),
  ),

  _rollback: $ => seq(
    $.keyword_rollback,
    optional(
      choice($.keyword_transaction, $.keyword_work),
    ),
  ),

};
