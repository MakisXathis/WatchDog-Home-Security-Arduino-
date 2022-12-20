#ifndef _BASE64_H
#define _BASE64_H

class Base64Class{
  public:
    int encode(char *output, const char *input, int inputLength);
    int decode(char * output, const char * input, int inputLength);
    int encodedLength(int plainLength);
    int decodedLength(const char * input, int inputLength);

  private:
    inline void fromA3ToA4(unsigned char * A4, unsigned char * A3);
    inline void fromA4ToA3(unsigned char * A3, unsigned char * A4);
    inline unsigned char lookupTable(char c);
};
extern Base64Class Base64;

#endif // _BASE64_H

