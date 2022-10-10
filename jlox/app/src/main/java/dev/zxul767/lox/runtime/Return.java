package dev.zxul767.lox.runtime;

class Return extends RuntimeException {
  final Object value;

  Return(Object value) {
    super(/*message:*/ null, /*cause:*/ null, /*enableSuppression:*/ false,
          /*writableStackTrace*/ false);
    this.value = value;
  }
}
