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

Coroutine_Proc :: #type (*void);

Coroutine_Status :: enum {
  DEAD;
  ACTIVE;
  SUSPENDED;
}

Coroutine :: struct {
  status: Coroutine_Status;
  fiber: *void;

  proc: Coroutine_Proc;
  param: *void;
  in: *void;
  out: *void;
}

co_create :: (proc: Coroutine_Proc, param: Any) -> Coroutine {
  if context.co_state == null {
    context.co_state = New(Coroutine_State);
    context.co_state.calling_fiber = ConvertThreadToFiber(null);
    context.co_state.active_coroutine.fiber = context.co_state.calling_fiber;
  }

  result: Coroutine;
  result.status = .SUSPENDED;
  result.fiber = CreateFiber(0, fiber_bootstrap, *context);
  result.proc = proc;
  result.param = param.value_pointer;

  return result;
}

co_resume :: (co: *Coroutine, in := Any.{}, out := null) {
  assert(context.co_state != null);
  if co.status == .DEAD then return;
  prev_caller := context.co_state.calling_fiber;
  defer context.co_state.calling_fiber = prev_caller;
  context.co_state.calling_fiber = context.co_state.active_coroutine.fiber;
  context.co_state.active_coroutine = co;
  co.status = .ACTIVE;
  co.in = in.value_pointer;
  co.out = out;
  SwitchToFiber(co.fiber);
  if co.status == .DEAD {
    co_delete(co);
  } else co.status = .SUSPENDED;
}

co_param :: inline () -> *void {
  assert(context.co_state != null);
  return context.co_state.active_coroutine.param;
}

co_input :: inline () -> *void {
  assert(context.co_state != null);
  return context.co_state.active_coroutine.in;
}

co_output :: inline () -> *void {
  assert(context.co_state != null);
  return context.co_state.active_coroutine.out;
}

co_yield :: () {
  assert(context.co_state != null);
  SwitchToFiber(context.co_state.calling_fiber);
}

co_delete :: (co: *Coroutine) {
  DeleteFiber(co.fiber);
  co.fiber = null;
}

#scope_file

Coroutine_State :: struct {
  calling_fiber: *void;
  active_coroutine: *Coroutine;
}
#add_context co_state: *Coroutine_State;

#if OS == .WINDOWS {
  kernel32 :: #library,system "kernel32";
  ConvertThreadToFiber :: (lpParameter: *void) -> *void #foreign kernel32;
  CreateFiber :: (dwStackSize: u64, lpStartAddress: (*void) -> () #c_call, lpParameter: *void) -> *void #foreign kernel32;
  SwitchToFiber :: (lpFiber: *void) #foreign kernel32;
  DeleteFiber :: (lpFiber: *void) #foreign kernel32;

  fiber_bootstrap :: (lpParameter: *void) #c_call {
    ctx_pointer := cast(*#Context)lpParameter;
    push_context ctx_pointer {
      co := context.co_state.active_coroutine;
      co.proc(co.param);
      co.status = .DEAD;
    }
  }
}

#scope_export

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