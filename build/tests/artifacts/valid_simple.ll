; Voltis LLVM backend output
; track: production-directed
source_filename = "valid_simple"
target triple = "x86_64-pc-windows-msvc"

declare i8* @vt.to_string.i32(i32)
declare void @vt.print.str(i8*)
declare void @vt.print.i32(i32)

define i32 @add(i32 %arg0, i32 %arg1) {
entry.alloca:
  %l0 = alloca i32
  %l1 = alloca i32
  store i32 %arg0, i32* %l0
  store i32 %arg1, i32* %l1
  br label %bb0

bb0:
  %v0 = load i32, i32* %l0
  %v1 = load i32, i32* %l1
  %v2 = add i32 %v0, %v1
  ret i32 %v2

}

define i32 @main() {
entry.alloca:
  %l0 = alloca i32
  %l1 = alloca i8*
  %l2 = alloca i32
  br label %bb0

bb0:
  store i32 5, i32* %l0
  %v1 = load i32, i32* %l0
  %v2 = call i8* @vt.to_string.i32(i32 %v1)
  store i8* %v2, i8** %l1
  %v3 = load i8*, i8** %l1
  call void @vt.print.str(i8* %v3)
  %v4 = load i32, i32* %l0
  %v6 = call i32 @add(i32 %v4, i32 2)
  store i32 %v6, i32* %l2
  %v7 = load i32, i32* %l2
  call void @vt.print.i32(i32 %v7)
  ret i32 0

}

