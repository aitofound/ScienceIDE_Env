module ModGridInfo

  use iso_c_binding

  implicit none

  integer, parameter:: iPicOn_ = 1, iPicOff_ = 0

  interface
     !------------------------------------------------
     subroutine reset_grid_info(Data, nx, ny, nz) bind(c)
       integer:: Data(*)
       integer, value:: nx, ny, nz
     end subroutine reset_grid_info

     !------------------------------------------------
     subroutine set_point_status(Data, nx, ny, nz, i, j, k, Val) bind(c)
       integer:: Data(*)
       integer, value:: nx, ny, nz, i, j, k, Val
     end subroutine set_point_status

     !------------------------------------------------
     subroutine get_point_status(Data, nx, ny, nz, i, j, k, Val) bind(c)
       integer:: Data(*)
       integer, value:: nx, ny, nz, i, j, k
       integer:: Val
     end subroutine get_point_status

  end interface

end module ModGridInfo
!==============================================================================
