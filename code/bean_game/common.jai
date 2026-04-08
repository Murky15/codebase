// NOTE: This parameter is until the language allows us to query the alignment of types (why it does not already support this is a mystery).
Arena :: struct {
  type: enum {
    MMU;
    FIXED;
  }
  align: int;
  pos: int;
  commit_pos: int;
  cap: int;
}

arena_alloc :: (buff: *void, size: int, align := 8) -> *Arena {
  assert(buff != null);
  result := cast(*Arena) buff;
  result.type = .FIXED;
  result.align = align;
  result.pos = size_of(Arena);
  result.commit_pos = size;
  result.cap = size;

  return result;
}

arena_push :: (arena: *Arena, size: int, $zero_mem := true) -> *void {
  assert(arena != null);
  result: *void;

  base := cast(*u8) arena;
  aligned_pos := round_up_pow2(arena.pos, arena.align);
  new_pos := aligned_pos + size;
  if new_pos <= arena.cap {
    result = base + aligned_pos;
    arena.pos = new_pos;
    #if zero_mem memset(result, 0, size);
  } else {
    // TODO: Handle this gracefully
    assert(false, "Arena limit reached! Desired allocation size: %. Space remaining in arena: %.", size, arena.cap - arena.pos);
  }

  return result;
}

arena_push :: (arena: *Arena, $T: Type, $initialized := true) -> *T {
  result := arena_push(arena, size_of(T));
  #if initialized {
    ini :: initializer_of(T);
    #if ini inline ini(result);
  }

  return cast(*T) result;
}

arena_push :: (arena: *Arena, count: int, $T: Type, $initialized := true) -> [] T {
  memory := arena_push(arena, size_of(T) * count);

  result: [] T;
  result.data = memory;
  result.count = count;

  #if initialized {
    ini :: initializer_of(T);
    #if ini {
      for 0..count-1 {
        inline ini(memory);
        memory += size_of(T);
      }
    }
  }

  return result;
}

arena_pop_to :: (arena: *Arena, pos: int) {
  assert(arena != null);

  min_pos := size_of(Arena);
  max_pos := arena.pos;
  new_pos := clamp(pos, min_pos, max_pos);
  arena.pos = new_pos;
}

arena_pop :: (arena: *Arena, amount: int) {
  new_pos := arena.pos - amount;
  arena_pop_to(arena, new_pos);
}

arena_clear :: (arena: *Arena) {
  arena_pop_to(arena, 0);
}

arena_pos :: (arena: *Arena) -> int {
  return arena.pos;
}

Linked_List :: struct (T: Type) {
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

/* NOTE: Fun experiment but not very practical.
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

co_create :: (proc: Coroutine_Proc, param := null) -> Coroutine {
  if context.co_state == null {
    context.co_state = New(Coroutine_State);
    context.co_state.calling_fiber = ConvertThreadToFiber(null);
  }

  result: Coroutine;
  result.status = .SUSPENDED;
  result.fiber = CreateFiber(0, fiber_bootstrap, *context);
  result.proc = proc;
  result.param = param;

  return result;
}

co_resume :: (co: *Coroutine, in := null, out := null) {
  assert(context.co_state != null);
  if co.status == .DEAD then return;
  prev_caller := context.co_state.calling_fiber;
  prev_active := context.co_state.active_coroutine;
  defer {
    context.co_state.calling_fiber = prev_caller;
    context.co_state.active_coroutine = prev_active;
  }
  context.co_state.calling_fiber = ifx prev_active == null prev_caller else prev_active.fiber;
  context.co_state.active_coroutine = co;
  co.status = .ACTIVE;
  co.in = in;
  co.out = out;
  SwitchToFiber(co.fiber);
  if co.status == .DEAD {
    co_delete(co);
  } else co.status = .SUSPENDED;
}

co_param :: inline () -> *void {
  assert(context.co_state != null && context.co_state.active_coroutine);
  return context.co_state.active_coroutine.param;
}

co_input :: inline () -> *void {
  assert(context.co_state != null && context.co_state.active_coroutine);
  return context.co_state.active_coroutine.in;
}

co_output :: inline () -> *void {
  assert(context.co_state != null && context.co_state.active_coroutine);
  return context.co_state.active_coroutine.out;
}

co_yield :: () {
  assert(context.co_state != null && context.co_state.active_coroutine);
  SwitchToFiber(context.co_state.calling_fiber);
}

co_delete :: (co: *Coroutine) {
  if co.fiber == null then return;
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
      SwitchToFiber(context.co_state.calling_fiber);
    }
  }
}

#scope_export
*/

round  :: x => ifx x - cast(int) x >= 0.5 then ceil(x) else floor(x);
sign   :: x => ifx x == 0 then 0 else ifx signbit(x) then -1 else 1;
norm   :: (x, min, max) => (x-min)/(max-min);

is_pow2 :: inline (x: int) -> bool {
  return x && !(x & (x - 1));
}

// TODO: Put assertions on these?
round_up_pow2 :: inline (x: int, pow2: int) -> int {
  return (x + pow2 - 1) & -pow2;
}

round_down_pow2 :: inline (x: int, pow2: int) -> int {
  return x & -pow2;
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