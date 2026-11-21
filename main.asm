extern getchar
extern malloc
extern putchar
global read_int
read_int:
  push rbp
  mov rbp, rsp
  sub rsp, 32
  mov rax, rbp
  sub rax, 8
  push rax
  mov rax, rsp
  and rax, 15
  jnz .Lcall0
  mov rax, 0
  call getchar wrt ..plt
  jmp .Lend0
.Lcall0:
  sub rsp, 8
  mov rax, 0
  call getchar wrt ..plt
  add rsp, 8
.Lend0:
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  mov rax, rbp
  sub rax, 16
  push rax
  push 0
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  mov rax, rbp
  sub rax, 24
  push rax
  push 0
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  mov rax, rbp
  sub rax, 8
  push rax
  pop rax
  mov rax, [rax]
  push rax
  push 45
  pop rdi
  pop rax
  cmp rax, rdi
  sete al
  movzx rax, al
  push rax
  pop rax
  cmp rax, 0
  je .Lend1
  mov rax, rbp
  sub rax, 24
  push rax
  push 1
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  mov rax, rbp
  sub rax, 8
  push rax
  mov rax, rsp
  and rax, 15
  jnz .Lcall2
  mov rax, 0
  call getchar wrt ..plt
  jmp .Lend2
.Lcall2:
  sub rsp, 8
  mov rax, 0
  call getchar wrt ..plt
  add rsp, 8
.Lend2:
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  push rax
.Lend1:
  pop rax
  mov rax, rbp
  sub rax, 32
  push rax
  push 0
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
.Lbegin3:
  push 48
  mov rax, rbp
  sub rax, 8
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  cmp rax, rdi
  setle al
  movzx rax, al
  push rax
  mov rax, rbp
  sub rax, 8
  push rax
  pop rax
  mov rax, [rax]
  push rax
  push 57
  pop rdi
  pop rax
  cmp rax, rdi
  setle al
  movzx rax, al
  push rax
  pop rdi
  pop rax
  imul rax, rdi
  push rax
  pop rax
  cmp rax, 0
  je .Lend3
  mov rax, rbp
  sub rax, 16
  push rax
  mov rax, rbp
  sub rax, 16
  push rax
  pop rax
  mov rax, [rax]
  push rax
  push 10
  pop rdi
  pop rax
  imul rax, rdi
  push rax
  mov rax, rbp
  sub rax, 8
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  add rax, rdi
  push rax
  push 48
  pop rdi
  pop rax
  sub rax, rdi
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  mov rax, rbp
  sub rax, 8
  push rax
  mov rax, rsp
  and rax, 15
  jnz .Lcall4
  mov rax, 0
  call getchar wrt ..plt
  jmp .Lend4
.Lcall4:
  sub rsp, 8
  mov rax, 0
  call getchar wrt ..plt
  add rsp, 8
.Lend4:
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  push rax
  mov rax, rbp
  sub rax, 32
  push rax
  mov rax, rbp
  sub rax, 32
  push rax
  pop rax
  mov rax, [rax]
  push rax
  push 1
  pop rdi
  pop rax
  add rax, rdi
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  jmp .Lbegin3
.Lend3:
  pop rax
  mov rax, rbp
  sub rax, 24
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rax
  cmp rax, 0
  je .Lend5
  mov rax, rbp
  sub rax, 16
  push rax
  push 0
  mov rax, rbp
  sub rax, 16
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  sub rax, rdi
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  push rax
.Lend5:
  pop rax
  mov rax, rbp
  sub rax, 16
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rax
  mov rsp, rbp
  pop rbp
  ret
  pop rax
  mov rsp, rbp
  pop rbp
  ret
global read_str
read_str:
  push rbp
  mov rbp, rsp
  sub rsp, 48
  mov rax, rbp
  sub rax, 8
  push rax
  push 0
  push 100
  pop rax
  push rax
  add rax, 1
  imul rax, 8
  mov rdi, rax
  mov rdi, rax
  mov rax, rsp
  and rax, 15
  jnz .Lcall6
  mov rax, 0
  call malloc wrt ..plt
  jmp .Lend6
.Lcall6:
  sub rsp, 8
  mov rax, 0
  call malloc wrt ..plt
  add rsp, 8
.Lend6:
  pop rcx
  mov [rax], rcx
  pop rsi
  add rax, 8
  push rax
  mov rdx, rax
.Lbegin7:
  cmp rcx, 0
  je .Lend7
  mov [rdx], rsi
  add rdx, 8
  sub rcx, 1
  jmp .Lbegin7
.Lend7:
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  mov rax, rbp
  sub rax, 16
  push rax
  push 0
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  mov rax, rbp
  sub rax, 24
  push rax
  mov rax, rsp
  and rax, 15
  jnz .Lcall8
  mov rax, 0
  call getchar wrt ..plt
  jmp .Lend8
.Lcall8:
  sub rsp, 8
  mov rax, 0
  call getchar wrt ..plt
  add rsp, 8
.Lend8:
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
.Lbegin9:
  push 32
  mov rax, rbp
  sub rax, 24
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  cmp rax, rdi
  setl al
  movzx rax, al
  push rax
  pop rax
  cmp rax, 0
  je .Lend9
  mov rax, rbp
  sub rax, 8
  push rax
  pop rax
  mov rax, [rax]
  push rax
  mov rax, rbp
  sub rax, 16
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  imul rdi, 8
  add rax, rdi
  push rax
  mov rax, rbp
  sub rax, 24
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  mov rax, rbp
  sub rax, 24
  push rax
  mov rax, rsp
  and rax, 15
  jnz .Lcall10
  mov rax, 0
  call getchar wrt ..plt
  jmp .Lend10
.Lcall10:
  sub rsp, 8
  mov rax, 0
  call getchar wrt ..plt
  add rsp, 8
.Lend10:
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  mov rax, rbp
  sub rax, 16
  push rax
  mov rax, rbp
  sub rax, 16
  push rax
  pop rax
  mov rax, [rax]
  push rax
  push 1
  pop rdi
  pop rax
  add rax, rdi
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  push rax
  jmp .Lbegin9
.Lend9:
  pop rax
  mov rax, rbp
  sub rax, 32
  push rax
  push 0
  mov rax, rbp
  sub rax, 16
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rax
  push rax
  add rax, 1
  imul rax, 8
  mov rdi, rax
  mov rdi, rax
  mov rax, rsp
  and rax, 15
  jnz .Lcall11
  mov rax, 0
  call malloc wrt ..plt
  jmp .Lend11
.Lcall11:
  sub rsp, 8
  mov rax, 0
  call malloc wrt ..plt
  add rsp, 8
.Lend11:
  pop rcx
  mov [rax], rcx
  pop rsi
  add rax, 8
  push rax
  mov rdx, rax
.Lbegin12:
  cmp rcx, 0
  je .Lend12
  mov [rdx], rsi
  add rdx, 8
  sub rcx, 1
  jmp .Lbegin12
.Lend12:
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  mov rax, rbp
  sub rax, 40
  push rax
  push 0
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
.Lbegin13:
  mov rax, rbp
  sub rax, 40
  push rax
  pop rax
  mov rax, [rax]
  push rax
  mov rax, rbp
  sub rax, 16
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  cmp rax, rdi
  setl al
  movzx rax, al
  push rax
  pop rax
  cmp rax, 0
  je .Lend13
  mov rax, rbp
  sub rax, 32
  push rax
  pop rax
  mov rax, [rax]
  push rax
  mov rax, rbp
  sub rax, 40
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  imul rdi, 8
  add rax, rdi
  push rax
  mov rax, rbp
  sub rax, 8
  push rax
  pop rax
  mov rax, [rax]
  push rax
  mov rax, rbp
  sub rax, 40
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  imul rdi, 8
  add rax, rdi
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  push rax
  mov rax, rbp
  sub rax, 40
  push rax
  mov rax, rbp
  sub rax, 40
  push rax
  pop rax
  mov rax, [rax]
  push rax
  push 1
  pop rdi
  pop rax
  add rax, rdi
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  jmp .Lbegin13
.Lend13:
  pop rax
  mov rax, rbp
  sub rax, 32
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rax
  mov rsp, rbp
  pop rbp
  ret
  pop rax
  mov rsp, rbp
  pop rbp
  ret
global print_int
print_int:
  push rbp
  mov rbp, rsp
  sub rsp, 48
  mov [rbp-8], rdi
  mov rax, rbp
  sub rax, 16
  push rax
  mov rax, rbp
  sub rax, 8
  push rax
  pop rax
  mov rax, [rax]
  push rax
  push 0
  pop rdi
  pop rax
  cmp rax, rdi
  setl al
  movzx rax, al
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  mov rax, rbp
  sub rax, 8
  push rax
  pop rax
  mov rax, [rax]
  push rax
  push 0
  pop rdi
  pop rax
  cmp rax, rdi
  sete al
  movzx rax, al
  push rax
  pop rax
  cmp rax, 0
  je .Lend14
  push 48
  pop rdi
  mov rax, rsp
  and rax, 15
  jnz .Lcall15
  mov rax, 0
  call putchar wrt ..plt
  jmp .Lend15
.Lcall15:
  sub rsp, 8
  mov rax, 0
  call putchar wrt ..plt
  add rsp, 8
.Lend15:
  push rax
  pop rax
  push 0
  pop rax
  mov rsp, rbp
  pop rbp
  ret
  pop rax
  push rax
.Lend14:
  pop rax
  mov rax, rbp
  sub rax, 16
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rax
  cmp rax, 0
  je .Lend16
  mov rax, rbp
  sub rax, 8
  push rax
  push 0
  mov rax, rbp
  sub rax, 8
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  sub rax, rdi
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
.Lend16:
  pop rax
  mov rax, rbp
  sub rax, 24
  push rax
  push 0
  push 20
  pop rax
  push rax
  add rax, 1
  imul rax, 8
  mov rdi, rax
  mov rdi, rax
  mov rax, rsp
  and rax, 15
  jnz .Lcall17
  mov rax, 0
  call malloc wrt ..plt
  jmp .Lend17
.Lcall17:
  sub rsp, 8
  mov rax, 0
  call malloc wrt ..plt
  add rsp, 8
.Lend17:
  pop rcx
  mov [rax], rcx
  pop rsi
  add rax, 8
  push rax
  mov rdx, rax
.Lbegin18:
  cmp rcx, 0
  je .Lend18
  mov [rdx], rsi
  add rdx, 8
  sub rcx, 1
  jmp .Lbegin18
.Lend18:
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  mov rax, rbp
  sub rax, 32
  push rax
  push 0
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
.Lbegin19:
  mov rax, rbp
  sub rax, 32
  push rax
  pop rax
  mov rax, [rax]
  push rax
  mov rax, rbp
  sub rax, 24
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rax
  mov rax, [rax-8]
  push rax
  pop rdi
  pop rax
  cmp rax, rdi
  setl al
  movzx rax, al
  push rax
  pop rax
  cmp rax, 0
  je .Lend19
  mov rax, rbp
  sub rax, 24
  push rax
  pop rax
  mov rax, [rax]
  push rax
  mov rax, rbp
  sub rax, 32
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  imul rdi, 8
  add rax, rdi
  push rax
  mov rax, rbp
  sub rax, 8
  push rax
  pop rax
  mov rax, [rax]
  push rax
  mov rax, rbp
  sub rax, 8
  push rax
  pop rax
  mov rax, [rax]
  push rax
  push 10
  pop rdi
  pop rax
  cqo
  idiv rdi
  push rax
  push 10
  pop rdi
  pop rax
  imul rax, rdi
  push rax
  pop rdi
  pop rax
  sub rax, rdi
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  mov rax, rbp
  sub rax, 8
  push rax
  mov rax, rbp
  sub rax, 8
  push rax
  pop rax
  mov rax, [rax]
  push rax
  push 10
  pop rdi
  pop rax
  cqo
  idiv rdi
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  push rax
  mov rax, rbp
  sub rax, 32
  push rax
  mov rax, rbp
  sub rax, 32
  push rax
  pop rax
  mov rax, [rax]
  push rax
  push 1
  pop rdi
  pop rax
  add rax, rdi
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  jmp .Lbegin19
.Lend19:
  pop rax
  mov rax, rbp
  sub rax, 40
  push rax
  push 0
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  mov rax, rbp
  sub rax, 16
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rax
  cmp rax, 0
  je .Lend20
  push 45
  pop rdi
  mov rax, rsp
  and rax, 15
  jnz .Lcall21
  mov rax, 0
  call putchar wrt ..plt
  jmp .Lend21
.Lcall21:
  sub rsp, 8
  mov rax, 0
  call putchar wrt ..plt
  add rsp, 8
.Lend21:
  push rax
  pop rax
  push rax
.Lend20:
  pop rax
  mov rax, rbp
  sub rax, 32
  push rax
  push 19
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
.Lbegin22:
  push 0
  mov rax, rbp
  sub rax, 32
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  cmp rax, rdi
  setle al
  movzx rax, al
  push rax
  pop rax
  cmp rax, 0
  je .Lend22
  mov rax, rbp
  sub rax, 24
  push rax
  pop rax
  mov rax, [rax]
  push rax
  mov rax, rbp
  sub rax, 32
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  imul rdi, 8
  add rax, rdi
  mov rax, [rax]
  push rax
  push 0
  pop rdi
  pop rax
  cmp rax, rdi
  setne al
  movzx rax, al
  push rax
  pop rax
  cmp rax, 0
  je .Lend23
  mov rax, rbp
  sub rax, 40
  push rax
  push 1
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  push rax
.Lend23:
  pop rax
  mov rax, rbp
  sub rax, 40
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rax
  cmp rax, 0
  je .Lend24
  mov rax, rbp
  sub rax, 24
  push rax
  pop rax
  mov rax, [rax]
  push rax
  mov rax, rbp
  sub rax, 32
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  imul rdi, 8
  add rax, rdi
  mov rax, [rax]
  push rax
  push 48
  pop rdi
  pop rax
  add rax, rdi
  push rax
  pop rdi
  mov rax, rsp
  and rax, 15
  jnz .Lcall25
  mov rax, 0
  call putchar wrt ..plt
  jmp .Lend25
.Lcall25:
  sub rsp, 8
  mov rax, 0
  call putchar wrt ..plt
  add rsp, 8
.Lend25:
  push rax
  pop rax
  push rax
.Lend24:
  pop rax
  push rax
  mov rax, rbp
  sub rax, 32
  push rax
  mov rax, rbp
  sub rax, 32
  push rax
  pop rax
  mov rax, [rax]
  push rax
  push 1
  pop rdi
  pop rax
  sub rax, rdi
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  jmp .Lbegin22
.Lend22:
  pop rax
  mov rsp, rbp
  pop rbp
  ret
global print_str
print_str:
  push rbp
  mov rbp, rsp
  sub rsp, 16
  mov [rbp-8], rdi
  mov rax, rbp
  sub rax, 16
  push rax
  push 0
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
.Lbegin26:
  mov rax, rbp
  sub rax, 16
  push rax
  pop rax
  mov rax, [rax]
  push rax
  mov rax, rbp
  sub rax, 8
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rax
  mov rax, [rax-8]
  push rax
  pop rdi
  pop rax
  cmp rax, rdi
  setl al
  movzx rax, al
  push rax
  pop rax
  cmp rax, 0
  je .Lend26
  mov rax, rbp
  sub rax, 8
  push rax
  pop rax
  mov rax, [rax]
  push rax
  mov rax, rbp
  sub rax, 16
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  imul rdi, 8
  add rax, rdi
  mov rax, [rax]
  push rax
  pop rdi
  mov rax, rsp
  and rax, 15
  jnz .Lcall27
  mov rax, 0
  call putchar wrt ..plt
  jmp .Lend27
.Lcall27:
  sub rsp, 8
  mov rax, 0
  call putchar wrt ..plt
  add rsp, 8
.Lend27:
  push rax
  pop rax
  push rax
  mov rax, rbp
  sub rax, 16
  push rax
  mov rax, rbp
  sub rax, 16
  push rax
  pop rax
  mov rax, [rax]
  push rax
  push 1
  pop rdi
  pop rax
  add rax, rdi
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  jmp .Lbegin26
.Lend26:
  pop rax
  mov rsp, rbp
  pop rbp
  ret
global main
main:
  push rbp
  mov rbp, rsp
  sub rsp, 16
  mov rax, rbp
  sub rax, 8
  push rax
  mov rax, rsp
  and rax, 15
  jnz .Lcall28
  mov rax, 0
  call read_int
  jmp .Lend28
.Lcall28:
  sub rsp, 8
  mov rax, 0
  call read_int
  add rsp, 8
.Lend28:
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  mov rax, rbp
  sub rax, 16
  push rax
  mov rax, rsp
  and rax, 15
  jnz .Lcall29
  mov rax, 0
  call read_int
  jmp .Lend29
.Lcall29:
  sub rsp, 8
  mov rax, 0
  call read_int
  add rsp, 8
.Lend29:
  push rax
  pop rdi
  pop rax
  mov [rax], rdi
  push rdi
  pop rax
  push 30
  mov rax, rbp
  sub rax, 8
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  sub rax, rdi
  push rax
  push 30
  mov rax, rbp
  sub rax, 16
  push rax
  pop rax
  mov rax, [rax]
  push rax
  pop rdi
  pop rax
  sub rax, rdi
  push rax
  pop rdi
  pop rax
  imul rax, rdi
  push rax
  pop rdi
  mov rax, rsp
  and rax, 15
  jnz .Lcall30
  mov rax, 0
  call print_int
  jmp .Lend30
.Lcall30:
  sub rsp, 8
  mov rax, 0
  call print_int
  add rsp, 8
.Lend30:
  push rax
  pop rax
  push 10
  pop rdi
  mov rax, rsp
  and rax, 15
  jnz .Lcall31
  mov rax, 0
  call putchar wrt ..plt
  jmp .Lend31
.Lcall31:
  sub rsp, 8
  mov rax, 0
  call putchar wrt ..plt
  add rsp, 8
.Lend31:
  push rax
  pop rax
  push 0
  pop rax
  mov rsp, rbp
  pop rbp
  ret
  pop rax
  mov rsp, rbp
  pop rbp
  ret
