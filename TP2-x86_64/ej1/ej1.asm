; === DEFINES ===
%define NULL 0
%define TRUE 1
%define FALSE 0

section .data
empty_str: db 0                ; Cadena vacía para concatenaciones iniciales

section .text

global string_proc_list_create_asm
global string_proc_node_create_asm
global string_proc_list_add_node_asm
global string_proc_list_concat_asm

extern malloc
extern free
extern str_concat

; ================================================================
; string_proc_list_create_asm
; Crea una lista vacía (estructura string_proc_list)
; ================================================================
string_proc_list_create_asm:
    push rbp
    mov rbp, rsp

    mov rdi, 16               ; Tamaño de la estructura (dos punteros)
    call malloc
    test rax, rax
    je .end_list_create       ; Si malloc falló, retorna NULL

    mov qword [rax], NULL     ; list->first = NULL
    mov qword [rax + 8], NULL ; list->last = NULL

.end_list_create:
    pop rbp
    ret

; ================================================================
; string_proc_node_create_asm
; Crea un nuevo nodo con tipo y puntero a hash
; ================================================================
string_proc_node_create_asm:
    push rbp
    mov rbp, rsp
    push r12
    push r13

    movzx r12, dil            ; r12 = type (byte a 64 bits)
    mov r13, rsi              ; r13 = hash (puntero a string)

    mov rdi, 32               ; Tamaño del nodo (4 punteros + 1 byte de type)
    call malloc
    test rax, rax
    je .end_node_create

    mov qword [rax], NULL         ; next = NULL
    mov qword [rax + 8], NULL     ; previous = NULL
    mov byte [rax + 16], r12b     ; type = type
    mov qword [rax + 24], r13     ; hash = hash

.end_node_create:
    pop r13
    pop r12
    pop rbp
    ret

; ================================================================
; string_proc_list_add_node_asm
; Agrega un nodo al final de la lista
; ================================================================
string_proc_list_add_node_asm:
    push rbp
    mov rbp, rsp
    sub rsp, 16             ; Shadow space para llamadas a funciones
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Guardar parámetros
    mov r12, rdi            ; r12 = list
    movzx r13, sil          ; r13 = type
    mov r14, rdx            ; r14 = hash

    ; Crear nuevo nodo
    mov rdi, r13
    mov rsi, r14
    call string_proc_node_create_asm
    mov r15, rax            ; r15 = new_node

    ; Verificar si el nodo se creó correctamente
    test r15, r15
    jz .ret_add             ; Si falla, retornar NULL

    test r12, r12
    jz .ret_add             ; Si la lista es NULL, retornar

    ; Verificar si la lista está vacía
    mov rbx, [r12]          ; rbx = list->first
    test rbx, rbx
    jz .empty_list

.not_empty:
    ; Enlazar nuevo nodo al final
    mov rbx, [r12 + 8]      ; rbx = list->last
    test rbx, rbx
    jz .ret_add             

    mov [r15 + 8], rbx      ; new_node->previous = list->last
    mov [rbx], r15          ; list->last->next = new_node
    mov [r12 + 8], r15      ; list->last = new_node
    jmp .ret_add

.empty_list:
    ; Lista vacía: setear first y last al nuevo nodo
    mov [r12], r15          ; list->first = new_node
    mov [r12 + 8], r15      ; list->last = new_node

.ret_add:
    mov rax, r15            ; retornar new_node

    ; Restaurar registros
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    add rsp, 16
    pop rbp
    ret

; ================================================================
; string_proc_list_concat_asm
; Concatena los hashes de los nodos que coinciden con el tipo dado
; junto con el hash inicial
; ================================================================
string_proc_list_concat_asm:
    push rbp
    mov rbp, rsp
    sub rsp, 16             ; Shadow space
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Guardar parámetros
    mov r15, rdi            ; r15 = list
    movzx r13, sil          ; r13 = type
    mov r14, rdx            ; r14 = initial_hash

    test r15, r15
    jz .return_initial_hash ; Si la lista es NULL, retornar initial_hash

    ; Inicializar result = str_concat("", initial_hash)
    mov rdi, empty_str
    mov rsi, r14
    call str_concat
    mov r12, rax            ; r12 = result

    ; Inicializar current = list->first
    mov rbx, [r15]
    test rbx, rbx
    jz .done

.loop:
    test rbx, rbx
    jz .done

    ; Comparar tipo
    movzx rax, byte [rbx + 16]
    cmp al, r13b
    jne .next

    ; Concatenar: result = str_concat(result, current->hash)
    mov rdi, r12
    mov rsi, [rbx + 24]
    call str_concat

    ; Liberar string anterior
    mov rdi, r12
    mov r12, rax
    call free

.next:
    ; current = current->next
    mov rbx, [rbx]
    jmp .loop

.done:
    mov rax, r12
    jmp .exit

.return_initial_hash:
    mov rdi, empty_str
    mov rsi, r14
    call str_concat

.exit:
    ; Restaurar registros
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    add rsp, 16
    pop rbp
    ret
