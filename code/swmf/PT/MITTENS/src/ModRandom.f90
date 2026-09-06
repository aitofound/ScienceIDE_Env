module PT_ModRandom
    use ModMpi
    use ModKind, ONLY: Int8_

    implicit none
    save

    logical, public :: UseInputSeedFile

    ! Portable pseudo-random number generator (xoshiro256+).
    ! The Fortran intrinsic random_number is compiler-specific, so results
    ! could not be reproduced across compilers/platforms. This generator
    ! produces the identical stream everywhere for a given seed, and each
    ! MPI rank gets a provably non-overlapping stream via the xoshiro jump
    ! function (streams are separated by 2^128 draws).

    ! Generator state of this rank
    integer(Int8_) :: RngState_I(4) = 0

    ! Mask for the low 32 bits
    integer(Int8_), parameter :: cMask32 = int(z'FFFFFFFF', Int8_)

    ! xoshiro256 jump polynomial, each constant built from two 32-bit
    ! halves to avoid overflowing signed 64-bit BOZ conversions
    integer(Int8_), parameter :: cJump_I(4) = [ &
        ior(ishft(int(z'180EC6D3',Int8_),32), int(z'3CFD0ABA',Int8_)), &
        ior(ishft(int(z'D5A61266',Int8_),32), int(z'F0C9392C',Int8_)), &
        ior(ishft(int(z'A9582618',Int8_),32), int(z'E03FC9AA',Int8_)), &
        ior(ishft(int(z'39ABDC45',Int8_),32), int(z'29B1661C',Int8_)) ]

contains
  !============================================================================

    subroutine read_param(NameCommand)

        use ModReadParam, ONLY: read_var
        use ModUtilities, ONLY: CON_stop

        character(len=*), intent(in):: NameCommand ! From PARAM.in
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
        select case(NameCommand)
        case('#INPUTSEED')
            call read_var("UseInputSeedFile", UseInputSeedFile)

        case default
            call CON_stop(NameSub//' Unknown command '//NameCommand)
        end select
    end subroutine read_param
  !============================================================================
    subroutine init
        ! Use input seed file or randomize PRNG seed
        ! Should be inside IsFirstSession?
    !--------------------------------------------------------------------------
        if(UseInputSeedFile) then
            call read_seed_file()
        else
            call init_random_seed()
        end if
        call save_seed() ! saves seed to file

    end subroutine init
  !============================================================================
    subroutine init_random_seed()
        use PT_ModProc, ONLY: iProc, iComm, iError
        use ModUtilities, ONLY: CON_stop

        integer :: MasterSeed_I(2) ! two default integers, MPI-portable
        integer :: fileUnit, istat
        integer(Int8_) :: MasterSeed

        ! rank 0 draws a master seed from /dev/urandom ...
    character(len=*), parameter:: NameSub = 'init_random_seed'
    !--------------------------------------------------------------------------
        if(iProc == 0) then
            open(newunit=fileUnit, file="/dev/urandom", access="stream", &
                 form="unformatted", action="read", status="old", iostat=istat)
            if (istat == 0) then
                read(fileUnit) MasterSeed_I
                close(fileUnit)
            else
                call CON_stop(NameSub//' /dev/urandom not found')
            end if
        end if

        ! ... and shares it, so all ranks start from the same master state
        call MPI_Bcast(MasterSeed_I, 2, MPI_INTEGER, 0, iComm, iError)

        MasterSeed = ior(ishft(int(MasterSeed_I(1), Int8_), 32), &
                         iand(int(MasterSeed_I(2), Int8_), cMask32))

        call init_state_of_rank(MasterSeed)

    end subroutine init_random_seed
  !============================================================================
    subroutine init_state_of_rank(MasterSeed)
        ! Set this rank's generator state from a shared master seed:
        ! expand the seed into the master state, then jump iProc times so
        ! each rank's stream is separated from all others by 2^128 draws
        use PT_ModProc, ONLY: iProc

        integer(Int8_), intent(in) :: MasterSeed

        integer(Int8_) :: x
        integer :: i, iJump
        real :: Dummy

    !--------------------------------------------------------------------------
        x = MasterSeed
        ! the xorshift64 scrambler below requires a nonzero start
        if(x == 0) x = ior(ishft(int(z'9E3779B9',Int8_),32), &
                           int(z'7F4A7C15',Int8_))

        ! expand one 64-bit seed into the four state words with xorshift64
        ! (xor/shift only: cannot overflow, never yields zero from nonzero)
        do i = 1, 4
            x = ieor(x, ishft(x, 13))
            x = ieor(x, ishft(x, -7))
            x = ieor(x, ishft(x, 17))
            RngState_I(i) = x
        end do

        ! warm-up: decorrelate from the low-entropy initial state
        do i = 1, 32
            Dummy = uniform_rand()
        end do

        ! separate this rank's stream from all lower ranks
        do iJump = 1, iProc
            call jump_stream
        end do

    end subroutine init_state_of_rank
  !============================================================================
    subroutine read_seed_file()
        ! Set the PRNG state for each processor from file seed.in.
        ! Two formats are accepted:
        ! (1) a single line with one integer: the master seed; every rank
        !     derives its own stream from it (used for frozen-seed tests)
        ! (2) one line per rank with 5 integers, as written by save_seed:
        !     iProc State1 State2 State3 State4   (used to continue a run)
        use PT_ModProc, ONLY: iProc, nProc, iComm, iError
        use ModUtilities, ONLY: CON_stop

        ! one extra slot: a genuine save_seed record has exactly 5 values,
        ! so a 6th value on the record means the file is not per-rank format
        integer(Int8_) :: Seed_I(6), MasterSeed
        ! state packed into default integers for MPI, as in save_seed
        integer :: StateBuf_I(8), StateMin_I(8), StateMax_I(8)
        integer :: io, nRecord
        logical :: IsPerRankFile, IsFound
        character(len=200) :: StringLine
    character(len=*), parameter:: NameSub = 'read_seed_file'
    !--------------------------------------------------------------------------
        IsPerRankFile = .false.
        IsFound = .false.
        nRecord = 0

        ! try the per-rank format first: one record per rank
        open(8, file = 'seed.in', action = 'read')
        do
            read(8, '(a)', iostat = io) StringLine
            if(io /= 0) EXIT
            if(len_trim(StringLine) == 0) CYCLE
            ! a record with fewer than 5 values is not per-rank format
            read(StringLine, *, iostat = io) Seed_I(1:5)
            if(io /= 0) EXIT
            IsPerRankFile = .true.
            nRecord = nRecord + 1
            ! a record with more than 5 values is not written by save_seed
            read(StringLine, *, iostat = io) Seed_I(1:6)
            if(io == 0) call CON_stop(NameSub// &
                ': seed.in record has more than 5 values; expected either'// &
                ' one master seed integer or per-rank lines '// &
                'iProc State1 State2 State3 State4 as written by save_seed')
            if(.not.IsFound .and. iProc == Seed_I(1)) then
                RngState_I = Seed_I(2:5)
                IsFound = .true.
            end if
        end do
        close(8)

        if(IsPerRankFile) then
            if(nRecord /= nProc .or. .not.IsFound) call CON_stop(NameSub// &
                ': per-rank seed.in does not have exactly one record'// &
                ' for each rank; regenerate it or use a single master seed')
            if(all(RngState_I == 0)) call CON_stop(NameSub// &
                ': seed.in gives an all-zero generator state')
            ! ranks must have distinct streams: identical states across
            ! ranks would make all ranks draw the same random sequence
            StateBuf_I = transfer(RngState_I, StateBuf_I)
            call MPI_Allreduce(StateBuf_I, StateMin_I, 8, MPI_INTEGER, &
                MPI_MIN, iComm, iError)
            call MPI_Allreduce(StateBuf_I, StateMax_I, 8, MPI_INTEGER, &
                MPI_MAX, iComm, iError)
            if(nProc > 1 .and. all(StateMin_I == StateMax_I)) &
                call CON_stop(NameSub// &
                ': all ranks got the identical state from seed.in;'// &
                ' the file is stale or malformed')
            RETURN
        end if

        ! master-seed format: one integer for all ranks
        open(8, file = 'seed.in', action = 'read')
        read(8, *, iostat = io) MasterSeed
        close(8)
        if(io /= 0) call CON_stop(NameSub// &
            ': cannot read a master seed from seed.in')
        call init_state_of_rank(MasterSeed)

    end subroutine read_seed_file
  !============================================================================
    subroutine save_seed()

        use PT_ModProc, ONLY: iProc, nProc, iComm, iError

        integer :: i
        integer(Int8_) :: State_I(4)
        ! the 64-bit state is gathered as pairs of default integers, since
        ! the checked MPI interfaces only cover default integer kinds
        integer :: SeedBuf_I(9)
        integer, allocatable :: AllSeedBuf_IP(:, :)

        ! first value is processor id, the rest is the generator state
    !--------------------------------------------------------------------------
        SeedBuf_I(1) = iProc
        SeedBuf_I(2:9) = transfer(RngState_I, SeedBuf_I(2:9))

        ! create output array
        allocate(AllSeedBuf_IP(9, nProc))

        ! combine all processor's states
        call MPI_Gather(SeedBuf_I, 9, MPI_INTEGER, AllSeedBuf_IP, &
                        9, MPI_INTEGER, 0, iComm, iError)

        if(iProc /= 0) RETURN

        ! write to file
        open(8, file = 'PT/IO2/seed.out', status='new', action='write')
        do i = 1, nProc
            State_I = transfer(AllSeedBuf_IP(2:9, i), State_I)
            write(8, *) AllSeedBuf_IP(1, i), State_I
        end do
        close(8)

    end subroutine save_seed
  !============================================================================
    subroutine advance_state()
        ! one xoshiro256 state transition
        integer(Int8_) :: t
    !--------------------------------------------------------------------------
        t = ishft(RngState_I(2), 17)
        RngState_I(3) = ieor(RngState_I(3), RngState_I(1))
        RngState_I(4) = ieor(RngState_I(4), RngState_I(2))
        RngState_I(2) = ieor(RngState_I(2), RngState_I(3))
        RngState_I(1) = ieor(RngState_I(1), RngState_I(4))
        RngState_I(3) = ieor(RngState_I(3), t)
        RngState_I(4) = i8_rotl(RngState_I(4), 45)
    end subroutine advance_state
  !============================================================================
    function uniform_rand() result(Rand)
        ! xoshiro256+ : uniform random number in [0, 1)
        real :: Rand
        integer(Int8_) :: Output
    !--------------------------------------------------------------------------
        Output = i8_add(RngState_I(1), RngState_I(4))
        call advance_state

        ! top 53 bits -> uniform in [0,1)
        ! The LOW bits of the xoshiro256+ output have known linear
        ! artifacts (the '+' scrambler is weak there); only the top bits
        ! may be used. Never derive random integers with mod() on the raw
        ! Output - always go through this uniform and scale.
        Rand = real(ishft(Output, -11)) * 2.0**(-53)

    end function uniform_rand
  !============================================================================
    subroutine jump_stream()
        ! Advance the state by 2^128 draws (xoshiro256 jump function)
        integer(Int8_) :: State_I(4)
        integer :: i, iBit
    !--------------------------------------------------------------------------
        State_I = 0
        do i = 1, 4
            do iBit = 0, 63
                if(btest(cJump_I(i), iBit)) &
                    State_I = ieor(State_I, RngState_I)
                call advance_state
            end do
        end do
        RngState_I = State_I

    end subroutine jump_stream
  !============================================================================
    function i8_add(a, b) result(c)
        ! 64-bit wrap-around addition without signed overflow
        ! (overflow is undefined in Fortran and trapped by some debug flags)
        ! WARNING: this split 32-bit emulation of unsigned arithmetic is
        ! load-bearing, not a style choice. "Simplifying" it to a plain
        ! a + b compiles everywhere but is non-standard on overflow: it
        ! traps under NAG -C and may differ between compilers, silently
        ! breaking cross-compiler bit reproducibility of every frozen-seed
        ! reference. Do not refactor.
        integer(Int8_), intent(in) :: a, b
        integer(Int8_) :: c
        integer(Int8_) :: cLo, cHi
    !--------------------------------------------------------------------------
        cLo = iand(a, cMask32) + iand(b, cMask32)
        cHi = iand(ishft(a, -32) + ishft(b, -32) + ishft(cLo, -32), cMask32)
        c = ior(ishft(cHi, 32), iand(cLo, cMask32))
    end function i8_add
  !============================================================================
    function i8_rotl(x, k) result(c)
        ! rotate the 64 bits of x left by k
        integer(Int8_), intent(in) :: x
        integer, intent(in) :: k
        integer(Int8_) :: c
    !--------------------------------------------------------------------------
        c = ior(ishft(x, k), ishft(x, k - 64))
    end function i8_rotl
  !============================================================================
    subroutine get_random_uniform(Rand)
        ! returns uniform random number in [0, 1)
        real, intent(out) :: Rand
    !--------------------------------------------------------------------------
        Rand = uniform_rand()
    end subroutine get_random_uniform
  !============================================================================
    subroutine get_random_normal(RandNormal1)
        use PT_ModConst, ONLY: cTwoPi
        ! returns random number sampled from normal distribution
        ! with mean = 0 and std = 1

        ! Box-Muller transformation limits the random variable to rn < ~6
        ! physically limiting the size of the random diffusive process

        ! can output two random normals if needed

        real, intent(out) :: RandNormal1 !, RandNormal2
        real :: RandUniform1, RandUniform2

        ! uniform random numbers over [0,1)
    !--------------------------------------------------------------------------
        RandUniform1 = uniform_rand()
        RandUniform2 = uniform_rand()

        ! redistribute to (0, 1] to avoid 0
        RandUniform1 = 1.0 - RandUniform1
        RandUniform2 = 1.0 - RandUniform2

        ! Box-Muller transformation
        ! two independent random variable with standard normal distribution
        ! only need one for 1-D version

        RandNormal1 = sqrt(-2.0*log(RandUniform1))*cos(cTwoPi*RandUniform2)
        ! RandNormal2 = sqrt(-2*log(RandUniform1))*sin(cTwoPi*RandUniform2)

    end subroutine get_random_normal
  !============================================================================
end module PT_ModRandom
!==============================================================================
