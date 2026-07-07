My_Struct_Compiled_On_2_029 :: struct {
  thing1: s8 = 4;
  thing2: string = "hello";
  thing3: [5] int;
}

main :: () {
  ms: My_Struct_Compiled_On_2_029;
  log("%", ms);

  tis := cast(*Type_Info_Struct) type_info(My_Struct_Compiled_On_2_029);
  log("%", tis.alignment);

}

#import "Basic";