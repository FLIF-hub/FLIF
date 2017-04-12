// This bit of code is based from some code from libwebp 0.5.0 (specifically from examples/pngdec.c)
// It is BSD-licensed


// Converts the NULL terminated 'hexstring' which contains 2-byte character
// representations of hex values to raw data.
// 'hexstring' may contain values consisting of [A-F][a-f][0-9] in pairs,
// e.g., 7af2..., separated by any number of newlines.
// 'expected_length' is the anticipated processed size.
// On success the raw buffer is returned with its length equivalent to
// 'expected_length'. NULL is returned if the processed length is less than
// 'expected_length' or any character aside from those above is encountered.
// The returned buffer must be freed by the caller.
static unsigned char* HexStringToBytes(const char* hexstring,
                                 size_t expected_length) {
  const char* src = hexstring;
  size_t actual_length = 0;
  unsigned char* const raw_data = (unsigned char*)malloc(expected_length);
  unsigned char* dst;

  if (raw_data == NULL) return NULL;

  for (dst = raw_data; actual_length < expected_length && *src != '\0'; ++src) {
    char* end;
    char val[3];
    if (*src == '\n') continue;
    val[0] = *src++;
    val[1] = *src;
    val[2] = '\0';
    *dst++ = (uint8_t)strtol(val, &end, 16);
    if (end != val + 2) break;
    ++actual_length;
  }

  if (actual_length != expected_length) {
    free(raw_data);
    return NULL;
  }
  return raw_data;
}

static int ProcessRawProfile(const char* profile, size_t profile_len,
                             unsigned char ** payload, size_t* payload_len) {
  const char* src = profile;
  char* end;
  int expected_length;

  if (profile == NULL || profile_len == 0) return 0;

  // ImageMagick formats 'raw profiles' as
  // '\n<name>\n<length>(%8lu)\n<hex payload>\n'.
  if (*src != '\n') {
    fprintf(stderr, "Malformed raw profile, expected '\\n' got '\\x%.2X'\n",
            *src);
    return 0;
  }
  ++src;
  // skip the profile name and extract the length.
  while (*src != '\0' && *src++ != '\n') {}
  expected_length = (int)strtol(src, &end, 10);
  if (*end != '\n') {
    fprintf(stderr, "Malformed raw profile, expected '\\n' got '\\x%.2X'\n",
            *end);
    return 0;
  }
  ++end;

  // 'end' now points to the profile payload.
  *payload = HexStringToBytes(end, expected_length);
  *payload_len = expected_length;
  if (*payload == NULL) return 0;
  return 1;
}