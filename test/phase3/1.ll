; ModuleID = '/home/benja/CS-201-Project/CS-201-Project/test/phase3/1.c'
source_filename = "/home/benja/CS-201-Project/CS-201-Project/test/phase3/1.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @test() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  %6 = alloca i32, align 4
  %7 = load i32, i32* %6, align 4
  store i32 %7, i32* %3, align 4
  %8 = load i32, i32* %5, align 4
  %9 = icmp sgt i32 %8, 0
  br i1 %9, label %10, label %17

10:                                               ; preds = %0
  %11 = load i32, i32* %1, align 4
  %12 = load i32, i32* %5, align 4
  %13 = sub nsw i32 %11, %12
  store i32 %13, i32* %2, align 4
  %14 = load i32, i32* %2, align 4
  %15 = load i32, i32* %3, align 4
  %16 = add nsw i32 %14, %15
  store i32 %16, i32* %5, align 4
  br label %21

17:                                               ; preds = %0
  %18 = load i32, i32* %2, align 4
  %19 = load i32, i32* %3, align 4
  %20 = add nsw i32 %18, %19
  store i32 %20, i32* %5, align 4
  br label %21

21:                                               ; preds = %17, %10
  %22 = load i32, i32* %2, align 4
  %23 = load i32, i32* %3, align 4
  %24 = add nsw i32 %22, %23
  store i32 %24, i32* %1, align 4
  %25 = load i32, i32* %3, align 4
  %26 = mul nsw i32 1, %25
  store i32 %26, i32* %6, align 4
  %27 = load i32, i32* %1, align 4
  %28 = load i32, i32* %3, align 4
  %29 = mul nsw i32 %27, %28
  store i32 %29, i32* %4, align 4
  %30 = load i32, i32* %2, align 4
  %31 = load i32, i32* %4, align 4
  %32 = sdiv i32 %30, %31
  store i32 %32, i32* %5, align 4
  ret void
}

attributes #0 = { noinline nounwind optnone uwtable "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 12.0.1"}
