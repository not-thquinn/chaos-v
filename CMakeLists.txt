cmake_minimum_required(VERSION 3.21)
project(ChaosV LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets)
option(CHAOSV_REQUIRE_MPFR "Fail configuration instead of using the long-double fallback" ON)
find_path(MPFR_INCLUDE_DIR mpfr.h)
find_library(MPFR_LIBRARY NAMES mpfr)
find_library(GMP_LIBRARY NAMES gmp)

add_library(chaos_v_physics STATIC
    src/physics.cpp
    src/physics.h
    src/period_tracker.h
)
target_include_directories(chaos_v_physics PUBLIC src)

add_library(chaos_v_ui STATIC
    src/mainwindow.cpp
    src/mainwindow.h
    src/precise_decimal.h
    src/precise_spinbox.cpp
    src/precise_spinbox.h
    src/fractal_shading.cpp
    src/fractal_shading.h
    src/render_schedule.cpp
    src/render_schedule.h
    src/simulation_export_dialog.cpp
    src/simulation_export_dialog.h
    src/sweep_dialog.cpp
    src/sweep_dialog.h
    src/views.cpp
    src/views.h
    src/zoom_dialog.cpp
    src/zoom_dialog.h
    src/zoom_math.cpp
    src/zoom_math.h
)
target_include_directories(chaos_v_ui PUBLIC src)
target_link_libraries(chaos_v_ui PUBLIC chaos_v_physics Qt6::Widgets)

qt_add_executable(chaos_v src/main.cpp)
set_target_properties(chaos_v PROPERTIES WIN32_EXECUTABLE TRUE)
target_link_libraries(chaos_v PRIVATE chaos_v_ui)

qt_add_executable(chaos_v_diagnose src/diagnose_main.cpp)
set_target_properties(chaos_v_diagnose PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(chaos_v_diagnose PRIVATE chaos_v_physics Qt6::Core)

add_executable(chaos_v_physics_regression tests/physics_regression.cpp)
target_link_libraries(chaos_v_physics_regression PRIVATE chaos_v_physics)
add_test(NAME physics_regression COMMAND chaos_v_physics_regression)

add_executable(chaos_v_physics_benchmark tests/physics_benchmark.cpp)
target_link_libraries(chaos_v_physics_benchmark PRIVATE chaos_v_physics)

add_executable(chaos_v_period_detection_benchmark
    tests/period_detection_benchmark.cpp)
target_link_libraries(chaos_v_period_detection_benchmark PRIVATE chaos_v_physics)

# Compile the same event engine with native floating point under private symbol
# names. The renderer can use it as a fast prepass without weakening the MPFR
# implementation or creating duplicate linker symbols.
add_library(chaos_v_native_probe STATIC
    src/physics.cpp
    src/physics.h
    src/native_probe.cpp
    src/native_probe.h
)
target_include_directories(chaos_v_native_probe PUBLIC src)
target_compile_definitions(chaos_v_native_probe PRIVATE
    CHAOSV_NATIVE_BACKEND=1
    Real=NativeReal
    V2=NativeV2
    Segment=NativeSegment
    Ball=NativeBall
    Sample=NativeSample
    AnimFrame=NativeAnimFrame
    Result=NativeResult
    Simulator=NativeSimulator
    setThreadRealPrecision=setNativeRealPrecision
    toDouble=nativeToDouble
    makeReal=makeNativeReal
    position=nativePosition
)

add_library(chaos_v_fractal_classifier STATIC
    src/fractal_classifier.cpp
    src/fractal_classifier.h
)
target_include_directories(chaos_v_fractal_classifier PUBLIC src)
target_link_libraries(chaos_v_fractal_classifier PUBLIC
    chaos_v_physics chaos_v_native_probe)
target_link_libraries(chaos_v_ui PUBLIC chaos_v_fractal_classifier)

add_executable(chaos_v_hybrid_regression tests/hybrid_regression.cpp)
target_link_libraries(chaos_v_hybrid_regression PRIVATE chaos_v_fractal_classifier)
add_test(NAME hybrid_regression COMMAND chaos_v_hybrid_regression)

add_executable(chaos_v_render_schedule_regression
    tests/render_schedule_regression.cpp
    src/render_schedule.cpp
)
target_include_directories(chaos_v_render_schedule_regression PRIVATE src)
add_test(NAME render_schedule_regression
    COMMAND chaos_v_render_schedule_regression)

add_executable(chaos_v_period_tracker_regression
    tests/period_tracker_regression.cpp)
target_include_directories(chaos_v_period_tracker_regression PRIVATE src)
add_test(NAME period_tracker_regression
    COMMAND chaos_v_period_tracker_regression)

add_executable(chaos_v_hybrid_benchmark tests/hybrid_benchmark.cpp)
target_link_libraries(chaos_v_hybrid_benchmark PRIVATE
    chaos_v_fractal_classifier)

add_executable(chaos_v_view_interaction_regression
    tests/view_interaction_regression.cpp)
target_link_libraries(chaos_v_view_interaction_regression PRIVATE chaos_v_ui)
add_test(NAME view_interaction_regression
    COMMAND chaos_v_view_interaction_regression)

add_executable(chaos_v_deep_zoom_precision_regression
    tests/deep_zoom_precision_regression.cpp)
target_link_libraries(chaos_v_deep_zoom_precision_regression PRIVATE
    chaos_v_physics Qt6::Core)
add_test(NAME deep_zoom_precision_regression
    COMMAND chaos_v_deep_zoom_precision_regression)

add_executable(chaos_v_zoom_math_regression
    tests/zoom_math_regression.cpp
    src/zoom_math.cpp)
target_include_directories(chaos_v_zoom_math_regression PRIVATE src)
target_link_libraries(chaos_v_zoom_math_regression PRIVATE
    chaos_v_physics Qt6::Core)
add_test(NAME zoom_math_regression COMMAND chaos_v_zoom_math_regression)

add_executable(chaos_v_fractal_shading_regression
    tests/fractal_shading_regression.cpp)
target_link_libraries(chaos_v_fractal_shading_regression PRIVATE chaos_v_ui)
add_test(NAME fractal_shading_regression
    COMMAND chaos_v_fractal_shading_regression)


if(MPFR_INCLUDE_DIR AND MPFR_LIBRARY AND GMP_LIBRARY)
    find_package(Boost REQUIRED)
    target_include_directories(chaos_v_physics PUBLIC ${Boost_INCLUDE_DIRS} ${MPFR_INCLUDE_DIR})
    target_link_libraries(chaos_v_physics PUBLIC ${MPFR_LIBRARY} ${GMP_LIBRARY})
    target_compile_definitions(chaos_v_physics PUBLIC CHAOSV_USE_MPFR=1)

    function(add_fixed_mpfr_probe target suffix digits10)
        add_library(${target} STATIC
            src/physics.cpp
            src/physics.h
            src/period_tracker.h
            src/fixed_probe.cpp
            src/fixed_probe.h
        )
        target_include_directories(${target} PUBLIC src)
        target_include_directories(${target} PRIVATE
            ${Boost_INCLUDE_DIRS} ${MPFR_INCLUDE_DIR})
        target_link_libraries(${target} PRIVATE ${MPFR_LIBRARY} ${GMP_LIBRARY})
        target_compile_definitions(${target} PRIVATE
            CHAOSV_USE_MPFR=1
            CHAOSV_FIXED_MPFR_DIGITS10=${digits10}
            CHAOSV_FIXED_PROBE_FUNCTION=runFixed${suffix}Probe
            Real=Fixed${suffix}Real
            V2=Fixed${suffix}V2
            Segment=Fixed${suffix}Segment
            Ball=Fixed${suffix}Ball
            Sample=Fixed${suffix}Sample
            AnimFrame=Fixed${suffix}AnimFrame
            Result=Fixed${suffix}Result
            Simulator=Fixed${suffix}Simulator
            setThreadRealPrecision=setFixed${suffix}RealPrecision
            toDouble=fixed${suffix}ToDouble
            makeReal=makeFixed${suffix}Real
            position=fixed${suffix}Position
        )
    endfunction()

    # These decimal digit counts map to 128, 257, and 513 MPFR bits. The
    # stack-backed types eliminate per-number significand allocations in the
    # fractal confirmation path while meeting each advertised binary band.
    add_fixed_mpfr_probe(chaos_v_fixed_128 128 38)
    add_fixed_mpfr_probe(chaos_v_fixed_257 257 77)
    add_fixed_mpfr_probe(chaos_v_fixed_513 513 154)
    target_link_libraries(chaos_v_fractal_classifier PRIVATE
        chaos_v_fixed_128 chaos_v_fixed_257 chaos_v_fixed_513)

    message(STATUS "Building with MPFR arbitrary-precision arithmetic")
elseif(CHAOSV_REQUIRE_MPFR)
    message(FATAL_ERROR "MPFR/GMP were not found. Install them or configure with CHAOSV_REQUIRE_MPFR=OFF.")
else()
    message(WARNING "MPFR/GMP not found: building the long-double fallback. Physics is event-driven but not arbitrary precision.")
endif()

install(TARGETS chaos_v chaos_v_diagnose RUNTIME DESTINATION bin)
