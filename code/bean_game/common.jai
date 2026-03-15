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
  kernel32 :: #library,system "kernel32";
  ConvertThreadToFiber :: (lpParameter: *void) -> *void #foreign kernel32;
  CreateFiber :: (dwStackSize: u64, lpStartAddress: (*void) -> () #c_call, lpParameter: *void) -> *void #foreign kernel32;
  DeleteFiber :: (lpFiber: *void) #foreign kernel32;
  SwitchToFiber :: (lpFiber: *void) #foreign kernel32;

  copy_any :: (x: Any) -> Any {
    any: Any = ---;
    any.type = x.type;
    any.value_pointer = alloc(x.type.runtime_size);
    memcpy(any.value_pointer, x.value_pointer, x.type.runtime_size);
    return any;
  }

  FIBER_BOOTSTRAP_GENERIC_SOURCE :: #string DONE
  for * co.final_output {
    it.* = copy_any(context.co_state.output[it_index]);
  }
  co.state = .DEAD;
  SwitchToFiber(context.co_state.home_fiber);
  DONE

  CO_RESUME_GENERIC_SOURCE :: #string DONE
  if co.state == .DEAD {
    context.co_state.output = array_copy(co.final_output,, temp);
  } else {
    co.state = .ACTIVE;
    context.co_state.active_fiber = co.fiber;
    array_add(*context.co_state.fiber_call_stack, co.fiber);
    SwitchToFiber(co.fiber);
    context.co_state.active_fiber = null;
    if co.state != .DEAD {
      co.state = .SUSPENDED;
    }
  }
  DONE

  #scope_file
  co_impl_generate_argument_header :: () #expand {
    for `tip.argument_types {
      print_to_builder(*`builder, "arg%: %", it_index, `Compiler.get_type(it));
      if it_index < `tip.argument_types.count-1 then print_to_builder(*`builder, ", ");
    }
  }

  co_impl_generate_return_header :: () #expand {
    for `tip.return_types {
      print_to_builder(*`builder, "ret%: %", it_index, `Compiler.get_type(it));
      if it_index < `tip.return_types.count-1 then print_to_builder(*`builder, ", ");
    }
  }

  co_impl_generate_input_array :: () #expand {
    print_to_builder(*`builder, "context.co_state.input = NewArray(%, Any,, temp);\n", `tip.argument_types.count);
    for `tip.argument_types {
      print_to_builder(*`builder, "context.co_state.input[%1] = arg%1;\n", it_index);
    }
  }

  co_impl_generate_output_array :: () #expand {
    print_to_builder(*`builder, "context.co_state.output = NewArray(%, Any,, temp);\n", `tip.return_types.count);
    for `tip.return_types {
      print_to_builder(*`builder, "context.co_state.output[%1] = ret%1;\n", it_index);
    }
  }

  co_impl_generate_output_collection :: () #expand {
    for `tip.return_types {
      t := `Compiler.get_type(it);
      print_to_builder(*`builder, "ret%1 := cast(*%2, context.co_state.output[%1].value_pointer).*;\n", it_index, t);
    }
  }

  co_impl_generate_input_collection :: () #expand {
    for `tip.argument_types {
      t := `Compiler.get_type(it);
      print_to_builder(*`builder, "arg%1 := cast(*%2, context.co_state.input[%1].value_pointer).*;\n", it_index, t);
    }
  }

  co_impl_generate_return_statement :: (prefix: string) #expand {
    print_to_builder(*`builder, "return");
    types := ifx prefix == "arg" then `tip.argument_types else `tip.return_types;
    for types {
      print_to_builder(*`builder, " %%", prefix, it_index);
      if it_index < types.count-1 then print_to_builder(*`builder, ",");
    }
    print_to_builder(*`builder, ";\n}\n");
  }
  #scope_export

  Coroutine_State :: struct {
    home_fiber: *void;
    active_fiber: *void;
    fiber_call_stack: [..] *void;
    input: [] Any;
    output: [] Any;
  }
  #add_context co_state: *Coroutine_State;

  Coroutine_Status :: enum {
    SUSPENDED;
    ACTIVE;
    DEAD;
  }

  Coroutine :: struct (T: Type) {
    proc: T;
    state := Coroutine_Status.SUSPENDED;
    final_output: [#run -> int {return cast(*Type_Info_Procedure,T).return_types.count;}] Any;

    fiber: *void;
    ctx: *#Context;

    #run,stallable {
      Compiler :: #import "Compiler";
      tip := cast(*Type_Info_Procedure) T;
      assert(tip.type == .PROCEDURE);
      builder: String_Builder;

      // NOTE: Generate co_resume
      proc_sig := replace(tprint("%", T), "procedure ", "");
      print_to_builder(*builder, "co_resume :: (co: *Coroutine(#type %)", proc_sig);
      if tip.argument_types.count > 0 then print_to_builder(*builder, ", ");
      co_impl_generate_argument_header();
      print_to_builder(*builder, ")");
      if tip.return_types.count > 0 {
        print_to_builder(*builder, " -> ");
      }
      co_impl_generate_return_header();
      print_to_builder(*builder, " {\n");
      co_impl_generate_input_array();
      print_to_builder(*builder, CO_RESUME_GENERIC_SOURCE);
      co_impl_generate_output_collection();
      co_impl_generate_return_statement("ret");

      // NOTE: Generate co_yield
      proc_sig = replace(tprint("%", T), "procedure ", "");
      print_to_builder(*builder, "co_yield :: (proc: %", proc_sig);
      if tip.return_types.count > 0 then print_to_builder(*builder, ", ");
      co_impl_generate_return_header();
      print_to_builder(*builder, ")");
      if tip.argument_types.count > 0 {
        print_to_builder(*builder, " -> ");
      }
      co_impl_generate_argument_header();
      print_to_builder(*builder, " {\n");
      co_impl_generate_output_array();
      // NOTE: This would mean that we are calling yield from the home fiber, which is illegal
      print_to_builder(*builder, "assert(context.co_state.active_fiber != null);\n");
      print_to_builder(*builder, "SwitchToFiber(context.co_state.home_fiber);\n");
      co_impl_generate_input_collection();
      co_impl_generate_return_statement("arg");

      Compiler.add_build_string(builder_to_string(*builder), -1);
    }

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
          if tip.return_types.count > 0 then print_to_builder(*builder, " := ");
          print_to_builder(*builder, "co.proc(");
          for tip.argument_types {
            t := cast(*Type, *it).*;
            print_to_builder(*builder, "cast(*%, context.co_state.input[%].value_pointer).*", t, it_index);
            if it_index < tip.argument_types.count-1 then print_to_builder(*builder, ", ");
          }
          print_to_builder(*builder, ");\n");
          co_impl_generate_output_array();
          print_to_builder(*builder, FIBER_BOOTSTRAP_GENERIC_SOURCE);

          return builder_to_string(*builder);
        }
      }
    }
  }

  co_create :: (proc: $T) -> *Coroutine(T) {
    if context.co_state == null {
      context.co_state = New(Coroutine_State);
      context.co_state.home_fiber = ConvertThreadToFiber(null);
      //array_add(*context.co_state.fiber_call_stack, context.co_state.home_fiber);
    }
    co := New(Coroutine(T));
    co.proc = proc;
    co.ctx = *context;
    co.fiber = CreateFiber(0, co.fiber_bootstrap, co);
    return co;
  }

  co_delete :: (co: *Coroutine($T)) {
    if co == null then return;
    assert(context.co_state.active_fiber != co.fiber);
    DeleteFiber(co.fiber);
    for co.final_output {
      free(it.value_pointer);
    }
    free(co);
  }

  co_query_status :: (co: *Coroutine($T)) -> Coroutine_Status {
    return co.state;
  }
}

round  :: x => ifx x - cast(int) x >= 0.5 then ceil(x) else floor(x);
square :: x => x * x;
sign   :: x => ifx x == 0 then 0 else ifx signbit(x) then -1 else 1;
norm   :: (x, min, max) => (x-min)/(max-min);

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