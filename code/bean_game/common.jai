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
  #scope_file
  #import "Windows";
  kernel32 :: #library,system "kernel32";
  ConvertThreadToFiber :: (lpParameter: *void) -> *void #foreign kernel32;
  CreateFiber :: (dwStackSize: u64, lpStartAddress: (*void) #c_call, lpParameter: *void) -> *void #foreign kernel32;
  SwitchToFiber :: (lpFiber: *void) #foreign kernel32;

  Coroutine :: struct(T: Type)
  #modify {
    ti := cast(*Type_Info) T;
    if ti.type != .PROCEDURE then return false, "Error creating coroutine: this parameter must be a procedure.";
    tip := cast(*Type_Info_Procedure) ti;
    if tip.return_types.count != 0 || tip.procedure_flags != 0 {
      return false, "Error creating coroutine: this procedure is invalid!";
    }
    return true;
  } {
    status: Coroutine_Status;
    proc: T;

    // NOTE: Inconvinient but must be bundled so we can send this to bootstrap_fiber
    fiber: *void;
    ctx: *#Context;
    args: [] Any;

    bootstrap_fiber :: (lpParameter: *void) #c_call {
      co := cast(*Coroutine(T)) lpParameter;
      defer SwitchToFiber(co.ctx.co_state.home);
      push_context co.ctx {
        tip :: cast(*Type_Info_Procedure) type_info(T);
        assert(co.args.count == tip.argument_types.count, "Error: Attempting to call the coroutine: %, with an improper number of arguments.", co.T);
        for co.args {
          if it.type.type != tip.argument_types[it_index].type ||
             it.type.runtime_size != tip.argument_types[it_index].runtime_size {
            assert(false, "Error: Attempting to call the coroutine: %, with an improper argument type: %.", co.T, it);
           }
        }

        // NOTE: Unpack arguments and call procedure
        #insert -> string {
          builder: String_Builder;
          print_to_builder(*builder, "co.proc(");
          for tip.argument_types {
            expected_type := cast(*Type, *it).*;
            print_to_builder(*builder, "cast(*%, co.args[%].value_pointer).*", expected_type, it_index);
            if it_index < tip.argument_types.count-1 then print_to_builder(*builder, ", ");
          }
          print_to_builder(*builder, ");");
          return builder_to_string(*builder);
        }


      }
    }
  }
  #scope_export

  Coroutine_State :: struct {
    home: *void;
    active: *void;
  }
  #add_context co_state: *Coroutine_State;

  co_create :: (coroutine: $T) -> *Coroutine(T) {
    if context.co_state == null {
      context.co_state = New(Coroutine_State);
      context.co_state.home = ConvertThreadToFiber(null);
    }

    result := New(Coroutine(T));
    result.fiber = CreateFiber(0, result.bootstrap_fiber, result);
    result.proc = coroutine;

    return result;
  }

  co_resume :: (co: *Coroutine, args: ..Any) {
    assert(context.co_state != null);
    co.status = .ACTIVE;
    context.co_state.active = co.fiber;
    defer context.co_state.active = null;
    co.ctx = *context;
    co.args = args;
    SwitchToFiber(co.fiber);
  }

  co_yield :: () {
    assert(context.home != context.active);
    SwitchToFiber(context.home);
  }

  Coroutine_Status :: enum {
    NULL;
    ACTIVE;
    SUSPENDED;
  }
  co_query_status :: (co: *Coroutine) -> Coroutine_Status {
    return co.status;
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