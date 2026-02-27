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

// NOTE: I'm trying this out because my mentor swears by coroutines, so let's see how much it actually cleans up my logic

#if OS == .WINDOWS {
  #import "Windows";
  kernel32 :: #library,system "kernel32";
  // NOTE: This parameter is pretty useless in Jai because we have no way to actually access this data.
  ConvertThreadToFiber :: (lpParameter: *void) -> *void #foreign kernel32;
}

Coroutine_State :: struct {
  platform_initialized: bool;
}
#add_context co_state: Coroutine_State;

co_resume :: (coroutine: $T, create_new_if_already_started := false, args: ..Any)
#modify {
  ti := cast(*Type_Info) T;
  return ti.type == .PROCEDURE, "Coroutine type must be a procedure!";
} {

}
co_yield :: () {

}
co_terminate :: () {

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