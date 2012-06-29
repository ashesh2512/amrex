
module cutcell_module

  implicit none

  type cutcell
     !
     ! For now we just contain our cell index.
     !
     integer :: cell(3)

  end type cutcell

  interface print
     module procedure cutcell_print
  end interface print

  type cutcell_container
     private
     integer                :: size = 0
     type(cutcell), pointer :: d(:) => NULL()
  end type cutcell_container

  interface build
     module procedure cutcell_container_build
  end interface build

  interface destroy
     module procedure cutcell_container_destroy
  end interface destroy

  interface size
     module procedure cutcell_container_size
  end interface size

  interface empty
     module procedure cutcell_container_empty
  end interface empty

  interface print
     module procedure cutcell_container_print
  end interface print
  !
  ! Gives direct access to the underlying vector of cutcells.
  !
  ! You can change/update cutcells using this interface.
  !
  interface dataptr
    module procedure cutcell_container_dataptr
  end interface dataptr

contains

  subroutine cutcell_print(p)
    type(cutcell), intent(in) :: p
    print*, 'cell = ', p%cell
  end subroutine cutcell_print

  subroutine cutcell_container_build(d,sz)
    type(cutcell_container), intent(out) :: d
    integer,                 intent(in ) :: sz
    if ( .not. associated(d%d) ) then
       d%size = sz
       allocate(d%d(d%size))
    end if
  end subroutine cutcell_container_build

  subroutine cutcell_container_destroy(d)
    type(cutcell_container), intent(inout) :: d
    if ( associated(d%d) ) then
       deallocate(d%d)
       d%size  = 0
       d%d     => Null()
    end if
  end subroutine cutcell_container_destroy

  pure function cutcell_container_empty(d) result(r)
    logical :: r
    type(cutcell_container), intent(in) :: d
    r = (d%size == 0)
  end function cutcell_container_empty

  pure function cutcell_container_size(d) result(r)
    integer :: r
    type(cutcell_container), intent(in) :: d
    r = d%size
  end function cutcell_container_size

  subroutine cutcell_container_print(d, str)
    type(cutcell_container), intent(in)           :: d
    character (len=*),        intent(in), optional :: str

    integer :: i

    if ( present(str) ) print*, str
    if ( empty(d) ) then
       print*, '"Empty"'
    else
       do i = 1, d%size
          call print(d%d(i))
       end do
    end if
  end subroutine cutcell_container_print

  function cutcell_container_dataptr(d) result(r)
    type(cutcell), pointer :: r(:)
    type(cutcell_container), intent(in) :: d
    r => d%d
  end function cutcell_container_dataptr

end module cutcell_module

! program main

!   use cutcell_module

!   implicit none

!   integer, parameter :: ngrids = 16

!   logical, allocatable :: own(:)

!   type(cutcell_container), allocatable :: cells(:)

!   type(cutcell), pointer :: r(:)

!   type(cutcell) :: cell

!   integer i,j

!   allocate(own(ngrids))
!   allocate(cells(ngrids))

!   own = .false.
!   !
!   ! We only own grids 3, 7 and 9.
!   !
!   own(3) = .true.
!   own(7) = .true.
!   own(9) = .true.

!   do i = 1, ngrids
!      if (.not. empty(cells(i))) print*, 'cutcell container should be empty'

!      if (own(i)) then
!         !
!         ! Allocate space for "i" cutcells in cells(i).
!         !
!         call build(cells(i), i)

!         if (empty(cells(i))) print*, 'cutcell container should NOT be empty'

!         r => dataptr(cells(i))

!         do j = 1,size(cells(i))
!            !
!            ! Populate the container.
!            !
!            cell%cell(1) = j
!            cell%cell(2) = j
!            cell%cell(3) = j

!            r(j) = cell
!         end do

!         print*, 'cutcell container @ index = ', i

!         call print(cells(i))

!      end if

!   end do
  
!   do i = 1, ngrids
!      call destroy(cells(i))
!   end do

!   deallocate(own)
!   deallocate(cells)

! end program main
