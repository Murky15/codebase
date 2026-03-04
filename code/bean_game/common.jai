Linked_List :: struct (T: Type)
#modify { // NOTE: Probably unnecessary, but I wanted the practice.
  error := sprint("Linked_List_Header must be the first member of %!", T);
  ti := cast(*Type_Info) T;
  if ti.type != .STRUCT then return false, error;
  tis := cast(*Type_Info_Struct) ti;
  header := tis.members[0];
  if header.type.type != .STRUCT || header.offset_in_bytes > 0 then return false, error;
  header_struct := cast(*Type_Info_Struct) header.type;
  if header_struct.name != "Linked_List_Header" then return false, error;
  return true;
} {
  first, last: *T;
  count: int;
}

Linked_List_Header :: struct {
  next: *#this;
}

for_expansion :: (list: Linked_List, body: Code, flags: For_Flags) #expand {
  #assert !(flags & .REVERSE);
  node := list.first;
  `it_index := 0;
  while node != null {
    assert(it_index < list.count, "The number of nodes in this list exceeds the list count!");
    defer {
      node = xx node.next;
      it_index += 1;
    }
    #if flags & .POINTER {
      `it := node;
    } else {
      `it := node.*;
    }
    #insert body;
  }
}
sll_queue_push :: (using list: *Linked_List, n: *$T/Linked_List_Header) {
  if first == null {
    first = n;
    last = n;
    n.next = null;
  } else {
    last.next = xx n;
    last = n;
    n.next = null;
  }
  list.count += 1;
}
sll_queue_push_front :: (using list: *Linked_List, n: *$T/Linked_List_Header) {
  if first == null {
    first = n;
    last = n;
    n.next = null;
  } else {
    n.next = xx first;
    first = n;
  }
  list.count += 1;
}
sll_queue_pop :: (using list: *Linked_List, n: *$T/Linked_List_Header) {
  if first == last {
    first = null;
    last = null;
  } else {
    first = xx first.next;
  }
  list.count -= 1;
}
sll_stack_push :: (using list: *Linked_List, n: *$T/Linked_List_Header) {
  n.next = xx first;
  first = n;
  list.count += 1;
}
sll_stack_pop :: (using list: *Linked_List, n: *$T/Linked_List_Header) {
  if first != null {
    first = xx first.next;
  }
  list.count -= 1;
}

#if OS == .WINDOWS {
  // TODO: Verify that the context works as intended inside a coroutine
  kernel32 :: #library,system "kernel32";
  ConvertThreadToFiber :: (lpParameter: *void) -> *void #foreign kernel32;
  CreateFiber :: (dwStackSize: u64, lpStartAddress: (*void) -> () #c_call, lpParameter: *void) -> *void #foreign kernel32;
  SwitchToFiber :: (lpFiber: *void) #foreign kernel32;

  CO_RESUME :: #string DONE
  co_resume :: (co: *Coroutine($T), %) -> % {
    SwitchToFiber(co.fiber);

  }
  DONE


  /*
  co_create
  co_resume(*Coroutine(T),inputs of T) -> returns of T
  co_yield(returns of T) -> inputs of T
  co_query_status
  */
  Coroutine_State :: struct {
    home_fiber: *void;
    active_fiber: *void;
    input: [] Any;
    output: [] Any;
  }
  #add_context co_state: *Coroutine_State;

  Coroutine :: struct (T: Type) {
    proc: T;

    fiber: *void;
    ctx: *#Context;

    fiber_bootstrap :: (lpParameter: *void) #c_call {
      co := cast(*Coroutine(T)) lpParameter;
      push_context co.ctx {
        // NOTE: Insert procedure call and populate output array
        #insert -> string {
          tip := cast(*Type_Info_Procedure) T;
          assert(tip.type == .PROCEDURE);
          builder: String_Builder;
          for tip.return_types {
            print_to_builder(*builder, "ret%", it_index);
            if it_index < tip.return_types.count-1 then print_to_builder(*builder, ", ");
          }
          print_to_builder(*builder, " := ");
          print_to_builder(*builder, "co.proc(");
          for tip.argument_types {
            t := cast(*Type, *it).*;
            print_to_builder(*builder, "cast(*%, context.co_state.input[%].value_pointer).*", t, it_index);
            if it_index < tip.argument_types.count-1 then print_to_builder(*builder, ", ");
          }
          print_to_builder(*builder, ");\n");
          print_to_builder(*builder, "context.co_state.output = NewArray(%, Any,, temp);\n", tip.return_types.count);
          for tip.return_types {
            print_to_builder(*builder, "context.co_state.output[%1] = ret%1;\n", it_index);
          }

          return builder_to_string(*builder);
        }
      }
    }
  }

  co_create :: (proc: $T) -> *Coroutine(T) {
    #run {
      Compiler :: #import "Compiler";
      tip := cast(*Type_Info_Procedure) T;
      assert(tip.type == .PROCEDURE);
      builder: String_Builder;

      print_to_builder(*builder, "co_resume :: (co: *Coroutine($T), ");
      for tip.argument_types {
        print_to_builder(*builder, "arg%: %", it_index, Compiler.get_type(it));
        if it_index < tip.argument_types.count-1 then print_to_builder(*builder, ", ");
      }
      print_to_builder(*builder, ") -> ");
      for tip.return_types {
        print_to_builder(*builder, "%", Compiler.get_type(it));
        if it_index < tip.return_types.count-1 then print_to_builder(*builder, ", ");
      }
      print_to_builder(*builder, " {\n");
      print_to_builder(*builder, "context.co_state.input = NewArray(%, Any,, temp);\n", tip.argument_types.count);
      for tip.argument_types {
        print_to_builder(*builder, "context.co_state.input[%1] = arg%1;\n", it_index);
      }
      print_to_builder(*builder, "SwitchToFiber(co.fiber);\n");
      for tip.return_types {
        t := Compiler.get_type(it);
        print_to_builder(*builder, "ret%1 := cast(*%2, context.co_state.output[%1].value_pointer).*;\n", it_index, t);
      }
      print_to_builder(*builder, "return ");
      for tip.return_types {
        print_to_builder(*builder, "ret%1", it_index);
        if it_index < tip.argument_types.count-1 then print_to_builder(*builder, ", ");
      }
      print_to_builder(*builder, ";\n}\n");

      Compiler.add_build_string(builder_to_string(*builder), -1);
    }
    if context.co_state == null {
      context.co_state = New(Coroutine_State);
      context.co_state.home_fiber = ConvertThreadToFiber(null);
    }
    co := New(Coroutine(T));
    co.proc = proc;
    co.fiber = CreateFiber(0, co.fiber_bootstrap, co);
    return co;
  }
}

sign :: inline (x: $T) -> T {
  if x == 0 return 0;
  return abs(x)/x;
}

norm :: inline (x: float, min: float, max: float) -> float {
  return (x-min)/(max-min);
}

get_digits_right_to_left_on_temp :: (x: int) -> [] int {
  push_allocator(temp);
  digits: [..] int;
  if x == 0 then array_add(*digits, 0);
  while x > 0 {
    digit := x % 10;
    array_add(*digits, digit);
    x /= 10;
  }

  return digits;
}