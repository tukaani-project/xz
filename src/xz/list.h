///////////////////////////////////////////////////////////////////////////////
//
/// \file       list.h
/// \brief      List information about .xz files
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

/// \brief      List information about the given .xz file
extern void list_file(file_pair *pair);


/// \brief      Show the totals after all files have been listed
extern void list_totals(void);
