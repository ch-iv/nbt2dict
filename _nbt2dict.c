#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdint.h>
#include <string.h>

#define TAG_END 0x00
#define TAG_BYTE 0x01
#define TAG_SHORT 0x02
#define TAG_INT 0x03
#define TAG_LONG 0x04
#define TAG_FLOAT 0x05
#define TAG_DOUBLE 0x06
#define TAG_BYTE_ARRAY 0x07
#define TAG_STRING 0x08
#define TAG_LIST 0x09
#define TAG_COMPOUND 0x0A
#define TAG_INT_ARRAY 0x0B
#define TAG_LONG_ARRAY 0x0C
#define TAG_TBD 0x0D

typedef struct {
  const uint8_t *data;
  size_t pos;
  size_t length;
  int little_endian;
} NBTParser;

static inline uint16_t swap16(uint16_t val) { return (val >> 8) | (val << 8); }

static inline uint32_t swap32(uint32_t val) {
  return ((val >> 24) & 0xff) | ((val << 8) & 0xff0000) |
         ((val >> 8) & 0xff00) | ((val << 24) & 0xff000000);
}

static inline uint64_t swap64(uint64_t val) {
  return ((uint64_t)swap32((uint32_t)val) << 32) |
         swap32((uint32_t)(val >> 32));
}

static int read_byte(NBTParser *parser, uint8_t *out) {
  if (parser->pos >= parser->length) {
    PyErr_SetString(PyExc_ValueError, "Unexpected end of data");
    return -1;
  }
  *out = parser->data[parser->pos++];
  return 0;
}

static uint16_t read_size(NBTParser *parser) {
  if (parser->pos + 2 > parser->length) {
    PyErr_SetString(PyExc_ValueError, "Unexpected end of data");
    return -1;
  }

  uint32_t size_le;
  memcpy(&size_le, parser->data + parser->pos, 2);
  parser->pos += 2;

  return swap16(size_le);
}

static int read_bytes(NBTParser *parser, void *out, size_t count) {
  if (parser->pos + count > parser->length) {
    PyErr_SetString(PyExc_ValueError, "Unexpected end of data");
    return -1;
  }

  memcpy(out, parser->data + parser->pos, count);
  parser->pos += count;
  return 0;
}

static int read_short(NBTParser *parser, int16_t *out) {
  uint16_t val;
  if (read_bytes(parser, &val, 2) < 0)
    return -1;
  if (!parser->little_endian)
    val = swap16(val);
  *out = (int16_t)val;
  return 0;
}

static int read_int(NBTParser *parser, int32_t *out) {
  uint32_t val;
  if (read_bytes(parser, &val, 4) < 0)
    return -1;
  val = swap32(val);
  *out = (int32_t)val;
  return 0;
}

static int read_long(NBTParser *parser, int64_t *out) {
  uint64_t val;
  if (read_bytes(parser, &val, 8) < 0)
    return -1;
  if (!parser->little_endian)
    val = swap64(val);
  *out = (int64_t)val;
  return 0;
}

static int read_float(NBTParser *parser, float *out) {
  uint32_t val;
  if (read_bytes(parser, &val, 4) < 0)
    return -1;
  if (!parser->little_endian)
    val = swap32(val);
  memcpy(out, &val, 4);
  return 0;
}

static int read_double(NBTParser *parser, double *out) {
  uint64_t val;
  if (read_bytes(parser, &val, 8) < 0)
    return -1;
  if (!parser->little_endian)
    val = swap64(val);
  memcpy(out, &val, 8);
  return 0;
}

static PyObject *read_string(NBTParser *parser) {
  uint16_t length = read_size(parser);

  if (length <= 0) {
    return PyUnicode_FromString("");
  }

  if (!parser->little_endian)
    length = swap16(length);

  if (parser->pos + length > parser->length) {
    PyErr_SetString(PyExc_ValueError, "Unexpected end of data");
    return NULL;
  }

  PyObject *str = PyUnicode_DecodeUTF8(
      (const char *)(parser->data + parser->pos), length, "ignore");
  parser->pos += length;
  return str;
}

static PyObject *read_tag_payload(NBTParser *parser, uint8_t tag_type);

static int read_tag_header(NBTParser *parser, uint8_t *tag_type,
                           PyObject **name) {
  if (read_byte(parser, tag_type) < 0)
    return -1;

  if (*tag_type == TAG_END) {
    *name = PyUnicode_FromString("");
    return 0;
  }

  *name = read_string(parser);
  if (*name == NULL)
    return -1;

  return 0;
}

static PyObject *read_tag_payload(NBTParser *parser, uint8_t tag_type) {
  if (tag_type == TAG_TBD) {
    if (read_byte(parser, &tag_type) < 0)
      return NULL;
  }

  switch (tag_type) {
  case TAG_END:
    Py_RETURN_NONE;

  case TAG_BYTE: {
    int8_t val;
    if (read_bytes(parser, &val, 1) < 0)
      return NULL;
    return PyLong_FromLong(val);
  }

  case TAG_SHORT: {
    int16_t val;
    if (read_short(parser, &val) < 0)
      return NULL;
    return PyLong_FromLong(val);
  }

  case TAG_INT: {
    int32_t val;
    if (read_int(parser, &val) < 0)
      return NULL;
    return PyLong_FromLong(val);
  }

  case TAG_LONG: {
    int64_t val;
    if (read_long(parser, &val) < 0)
      return NULL;
    return PyLong_FromLongLong(val);
  }

  case TAG_FLOAT: {
    float val;
    if (read_float(parser, &val) < 0)
      return NULL;
    return PyFloat_FromDouble(val);
  }

  case TAG_DOUBLE: {
    double val;
    if (read_double(parser, &val) < 0)
      return NULL;
    return PyFloat_FromDouble(val);
  }

  case TAG_BYTE_ARRAY: {
    int32_t length;
    if (read_int(parser, &length) < 0)
      return NULL;

    PyObject *list = PyList_New(length);
    if (!list)
      return NULL;

    for (int32_t i = 0; i < length; i++) {
      int8_t val;
      if (read_bytes(parser, &val, 1) < 0) {
        Py_DECREF(list);
        return NULL;
      }
      PyList_SET_ITEM(list, i, PyLong_FromLong(val));
    }
    return list;
  }

  case TAG_STRING:
    return read_string(parser);

  case TAG_LIST: {
    uint8_t elem_type;
    int32_t length;

    if (read_byte(parser, &elem_type) < 0)
      return NULL;
    if (read_int(parser, &length) < 0)
      return NULL;

    if (length < 0) {
      PyErr_Format(PyExc_ValueError, "Invalid list length: %d", length);
      return NULL;
    }

    if (elem_type == TAG_END && length != 0) {
      PyErr_SetString(PyExc_ValueError,
                      "List has element type TAG_End but non-zero length");
      return NULL;
    }

    PyObject *list = PyList_New(length);
    if (!list)
      return NULL;

    for (int32_t i = 0; i < length; i++) {
      PyObject *item = read_tag_payload(parser, elem_type);
      if (!item) {
        Py_DECREF(list);
        return NULL;
      }
      PyList_SET_ITEM(list, i, item);
    }

    return list;
  }

  case TAG_COMPOUND: {
    PyObject *dict = PyDict_New();
    if (!dict)
      return NULL;

    while (1) {
      uint8_t child_tag;
      if (read_byte(parser, &child_tag) < 0) {
        Py_DECREF(dict);
        return NULL;
      }

      if (child_tag == TAG_END) {
        break;
      }

      PyObject *child_name = read_string(parser);
      if (!child_name) {
        Py_DECREF(dict);
        return NULL;
      }

      PyObject *value = read_tag_payload(parser, child_tag);
      if (!value) {
        Py_DECREF(child_name);
        Py_DECREF(dict);
        return NULL;
      }

      if (PyDict_SetItem(dict, child_name, value) < 0) {
        Py_DECREF(value);
        Py_DECREF(child_name);
        Py_DECREF(dict);
        return NULL;
      }

      Py_DECREF(child_name);
      Py_DECREF(value);
    }

    return dict;
  }
  case TAG_INT_ARRAY: {
    int32_t length;
    if (read_int(parser, &length) < 0)
      return NULL;

    PyObject *list = PyList_New(length);
    if (!list)
      return NULL;

    for (int32_t i = 0; i < length; i++) {
      int32_t val;
      if (read_int(parser, &val) < 0) {
        Py_DECREF(list);
        return NULL;
      }
      PyList_SET_ITEM(list, i, PyLong_FromLong(val));
    }
    return list;
  }

  case TAG_LONG_ARRAY: {
    int32_t length;
    if (read_int(parser, &length) < 0)
      return NULL;

    PyObject *list = PyList_New(length);
    if (!list)
      return NULL;

    for (int32_t i = 0; i < length; i++) {
      int64_t val;
      if (read_long(parser, &val) < 0) {
        Py_DECREF(list);
        return NULL;
      }
      PyList_SET_ITEM(list, i, PyLong_FromLongLong(val));
    }
    return list;
  }

  default:
    PyErr_Format(PyExc_ValueError, "Unknown tag type: %d", tag_type);
    return NULL;
  }
}

static PyObject *parse(PyObject *self, PyObject *args) {
  Py_buffer data;
  if (!PyArg_ParseTuple(args, "y*", &data)) {
    return NULL;
  }

  NBTParser parser;
  parser.data = (const uint8_t *)data.buf;
  parser.length = data.len;
  parser.pos = 0;
  parser.little_endian = 1;

  uint8_t root_type;
  if (read_byte(&parser, &root_type) < 0) {
    PyBuffer_Release(&data);
    return NULL;
  }

  PyObject *root_name = read_string(&parser);
  if (!root_name) {
    PyBuffer_Release(&data);
    return NULL;
  }

  Py_DECREF(root_name);

  PyObject *result = read_tag_payload(&parser, root_type);
  PyBuffer_Release(&data);
  return result;
}

static PyMethodDef methods[] = {
    {"parse", (PyCFunction)parse, METH_VARARGS,
     "Parse NBT binary data and return Python dictionary"},
    {NULL, NULL, 0, NULL}};

static struct PyModuleDef module = {PyModuleDef_HEAD_INIT, "_nbt2dict",
                                    "Fast NBT parser C extension for Python",
                                    -1, methods};

PyMODINIT_FUNC PyInit__nbt2dict(void) { return PyModule_Create(&module); }
