## Datatypes

- **byte**: 8 bit
- **short**: 16 bit
- **int**: 32 bit

## Header

1. Read the magic bytes `"FLIF"`
   
2. Read an unsigned byte and interpret it as follows:
   
   - *bits 0 - 3*: `numPlanes`
     
     **FIXME info:** valid range?
     
   - *bit 4*: `encoding`: (`0` => interlaced, `1` => non-interlaced)
     
   - *bit 5 - 6*: `animationType`: (`10` => multi-frame, `01` => single-frame)
     
   - *bit 7*: `0` 
   
3. *if mutli-frame*: read an unsigned byte and assign to `numFrames`, otherwise assume `numFrames == 1`
   
   **FIXME info:** valid range: `0` allowed? `1` allowed? `255` allowed? 
   
4. Read an unsigned byte `depthType`: (`'1'` => 8 bit, `'2'` => 16 bit, `'0'` => dynamic)
   
5. Read unsigned big endian short: `width`
   
6. Read unsigned big endian short: `height` 
   
7. Initialize *MANIAC* decoder