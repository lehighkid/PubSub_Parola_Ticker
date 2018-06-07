#ifdef DEBUG
  #define DPRINT(...)    Serial.print(__VA_ARGS__)
  //OR, #define DPRINT(args...)    Serial.print(args)
  #define DPRINTLN(...)  Serial.println(__VA_ARGS__)
  #define DPRINTF(...)    Serial.print(F(__VA_ARGS__))
  #define DPRINTLNF(...) Serial.println(F(__VA_ARGS__)) //printing text using the F macro
#else
  #define DPRINT(...)     //blank line
  #define DPRINTLN(...)   //blank line
  #define DPRINTF(...)    //blank line
  #define DPRINTLNF(...)  //blank line
#endif
