/* compile with:
   gcc -O2 -o greedy greedy_reconstruction.c
   Group members: [Sean], [Fariha], [Abdullah]
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

/* ─────────────────────────────────────────────────────
   PART 1: Fragment linked list
   (reused from the provided brute-force example)
   ───────────────────────────────────────────────────── */

struct fragment_s {
  struct fragment_s * next_fragment;
  char * fragment_string;
};

struct fragment_s *
read_all_fragments( char const * file_name )
{
  FILE * input = stdin;
  if( strcmp( file_name, "-" ) != 0 )
    input = fopen( file_name, "r" );
  if( input == NULL ) {
    fprintf( stderr, "Error: could not open file: %s\n", file_name );
    return NULL;
  }
  struct fragment_s * top_fragment = NULL;
  while( 1 ) {
    char * line_buffer = NULL;
    size_t buffer_size = 0;
    ssize_t read_bytes = getline( &line_buffer, &buffer_size, input );
    if( read_bytes <= 0 ) {
      if( line_buffer != NULL ) free( line_buffer );
      break;
    }
    for( int i = 0; i < read_bytes; i++ )
      if( line_buffer[i] == '\n' || line_buffer[i] == '\r' )
        line_buffer[i] = '\0';
    struct fragment_s * new_fragment = malloc( sizeof( struct fragment_s ));
    new_fragment->next_fragment = top_fragment;
    new_fragment->fragment_string = line_buffer;
    line_buffer = NULL;
    top_fragment = new_fragment;
  }
  fclose( input );
  return top_fragment;
}

void
free_all_fragments( struct fragment_s * top_fragment )
{
  while( top_fragment != NULL ) {
    struct fragment_s * this_fragment = top_fragment;
    top_fragment = this_fragment->next_fragment;
    free( this_fragment->fragment_string );
    free( this_fragment );
  }
}

/* ─────────────────────────────────────────────────────
   PART 2: Convert linked list → array
   ───────────────────────────────────────────────────── */

/* count how many fragments are in the linked list */
int count_fragments( struct fragment_s * top )
{
  int count = 0;
  while( top != NULL ) {
    count++;
    top = top->next_fragment;
  }
  return count;
}

/* copy fragment strings into a plain char** array */
char **
list_to_array( struct fragment_s * top, int n )
{
  char ** arr = malloc( n * sizeof(char *) );
  assert( arr != NULL );
  for( int i = n - 1; i >= 0; i-- ) {
    arr[i] = strdup( top->fragment_string );   
    top = top->next_fragment;
  }
  return arr;
}

void
free_array( char ** arr, int n )
{
  for( int i = 0; i < n; i++ )
    free( arr[i] );
  free( arr );
}

/* ─────────────────────────────────────────────────────
   PART 3: compute_overlap(A, B)

   Returns the length of the longest string that is
   both a suffix of A and a prefix of B.
   e.g. compute_overlap("abr", "bra") = 2
   ───────────────────────────────────────────────────── */

int compute_overlap( const char *A, const char *B )
{
  int lenA = strlen(A);
  int lenB = strlen(B);
  int max_possible = (lenA < lenB) ? lenA : lenB;

  for( int k = max_possible; k >= 1; k-- ) {
    /* compare last k chars of A with first k chars of B */
    if( strncmp( A + lenA - k, B, k ) == 0 )
      return k;
  }
  return 0;
}

/* ─────────────────────────────────────────────────────
   PART 4: remove_contained(arr, n) → new_n

   Removes any fragment that is a substring of another.
   e.g. if "ac" is already inside "abac", drop "ac".
   Returns the new count; arr is compacted in-place.
   ───────────────────────────────────────────────────── */

int remove_contained( char ** arr, int n )
{
  bool * drop = calloc( n, sizeof(bool) );  /* drop[i] = true means remove it */
  assert( drop != NULL );

  for( int i = 0; i < n; i++ ) {
    for( int j = 0; j < n; j++ ) {
      if( i == j ) continue;
      /* if arr[i] appears inside arr[j], mark i for removal */
      if( strstr( arr[j], arr[i] ) != NULL ) {
       size_t len_i = strlen( arr[i] );
       size_t len_j = strlen( arr[j] );

      if( len_i < len_j || ( len_i == len_j && i > j ) ) {
       drop[i] = true;
       break;
        }
      }
    }
  }

  /* Compress the array: Move the remaining fragments to the front of the array.*/
  int new_n = 0;
  for( int i = 0; i < n; i++ ) {
    if( !drop[i] ) {
      arr[new_n] = arr[i];   /* keep this one */
      new_n++;
    } else {
      free( arr[i] );        /* discard this one */
      arr[i] = NULL;
    }
  }

  free( drop );
  return new_n;   /* New fragment count after removal */
}

/* ─────────────────────────────────────────────────────
   PART 5: build_overlap_matrix(arr, n)

   Builds an n×n matrix where matrix[i][j] = overlap
   between arr[i] (as prefix-provider) and arr[j]
   (as suffix-provider). matrix[i][i] = 0 always.
   ───────────────────────────────────────────────────── */

int **
build_overlap_matrix( char ** arr, int n )
{
  /* allocate n rows */
  int ** matrix = malloc( n * sizeof(int *) );
  assert( matrix != NULL );

  for( int i = 0; i < n; i++ ) {
    /* allocate n columns for each row */
    matrix[i] = malloc( n * sizeof(int) );
    assert( matrix[i] != NULL );

    for( int j = 0; j < n; j++ ) {
      if( i == j )
        matrix[i][j] = 0;          /* Fragments cannot overlap with themselves */
      else
        matrix[i][j] = compute_overlap( arr[i], arr[j] );
    }
  }
  return matrix;
}

void
free_matrix( int ** matrix, int n )
{
  for( int i = 0; i < n; i++ )
    free( matrix[i] );
  free( matrix );
}

/* ─────────────────────────────────────────────────────
   PART 6: greedy_merge(A, B, overlap)

   Merges two fragments given their overlap length.
   e.g. merge("abr", "bra", 2) = "abra"
   ───────────────────────────────────────────────────── */

char *
merge_two( const char *A, const char *B, int overlap )
{
  int lenA = strlen(A);
  int lenB = strlen(B);
  int new_len = lenA + lenB - overlap;

  char * result = malloc( new_len + 1 );
  assert( result != NULL );

  memcpy( result, A, lenA );                    
  memcpy( result + lenA, B + overlap, lenB - overlap ); 
  result[new_len] = '\0';
  return result;
}

/* ─────────────────────────────────────────────────────
   PART 7: greedy_reconstruct(arr, n)

   Main greedy algorithm:
   1. Find the pair of elements (i, j) with the largest overlap.
   2. Merge them into one new fragment
   3. Repeat until only one fragment remains
   ───────────────────────────────────────────────────── */

char *
greedy_reconstruct( char ** arr, int n )
{
  /* Perform the operation on the local copy so that we don't corrupt the original copy.*/
  char ** frags = malloc( n * sizeof(char *) );
  assert( frags != NULL );
  for( int i = 0; i < n; i++ )
    frags[i] = strdup( arr[i] );

  int count = n;   /* Number of remaining fragments */

  while( count > 1 ) {

    /* find the pair with the largest overlap */
    int best_i = 0, best_j = 1, best_overlap = -1;
    for( int i = 0; i < count; i++ ) {
      for( int j = 0; j < count; j++ ) {
        if( i == j ) continue;
        int ov = compute_overlap( frags[i], frags[j] );
        if( ov > best_overlap ) {
          best_overlap = ov;
          best_i = i;
          best_j = j;
        }
      }
    }

    /* merge best_i and best_j into a new fragment */
    char * merged = merge_two( frags[best_i], frags[best_j], best_overlap );

    /* free the two merged fragments */
    free( frags[best_i] );
    free( frags[best_j] );

    /* Place the merged data in the position of best_i,
       and fill the blank left in best_j with the last fragment. */
    frags[best_i] = merged;
    frags[best_j] = frags[count - 1];
    count--;
  }

  /* The only remaining fragment is our answer. */
  char * result = frags[0];
  free( frags );   /* free the array but NOT frags[0] — caller owns it */
  return result;
}

/* ─────────────────────────────────────────────────────
   PART 8: greedy_order(arr, n)

   Returns the n fragments reordered according to
   greedy's merge sequence, WITHOUT actually merging
   them. This gives 2-opt a better starting point.
   ───────────────────────────────────────────────────── */

char **
greedy_order( char ** arr, int n )
{
  int * chain_tail = malloc( n * sizeof(int) );  /* tail fragment of each chain */
  int * chain_next = malloc( n * sizeof(int) );  /* next fragment in chain (-1 = end) */
  int * active     = malloc( n * sizeof(int) );  /* 1 if this index is a chain head */

  for( int i = 0; i < n; i++ ) {
    chain_tail[i] = i;
    chain_next[i] = -1;
    active[i]     = 1;
  }

  int count = n;
  while( count > 1 ) {
    int best_i = -1, best_j = -1, best_ov = -1;

    for( int i = 0; i < n; i++ ) {
      if( !active[i] ) continue;
      for( int j = 0; j < n; j++ ) {
        if( !active[j] || j == i ) continue;
        int ov = compute_overlap( arr[chain_tail[i]], arr[j] );
        if( ov > best_ov ) {
          best_ov = ov;
          best_i  = i;
          best_j  = j;
        }
      }
    }

    /* Append the chain best_j to the end of the chain best_i. */
    chain_next[chain_tail[best_i]] = best_j;
    chain_tail[best_i] = chain_tail[best_j];
    active[best_j] = 0;
    count--;
  }

  /* find surviving chain head */
  int head = 0;
  while( !active[head] ) head++;

  /* walk the chain and collect fragments in the order */
  char ** result = malloc( n * sizeof(char *) );
  int pos = 0, cur = head;
  while( cur != -1 ) {
    result[pos++] = strdup( arr[cur] );
    cur = chain_next[cur];
  }

  free( chain_tail );
  free( chain_next );
  free( active );
  return result;
}

/* ─────────────────────────────────────────────────────
   PART 9: two_opt_improve(arr, n)

   Takes a fragment ordering and repeatedly swaps pairs
   until no swap produces a shorter merged string.
   ───────────────────────────────────────────────────── */

/* Calculate the total length of sequentially merging arr[0..n-1]. */
int compute_total_length( char ** arr, int n )
{
  int total = strlen( arr[0] );
  for( int i = 1; i < n; i++ ) {
    int ov = compute_overlap( arr[i-1], arr[i] );
    total += strlen( arr[i] ) - ov;
  }
  return total;
}

/* merge all fragments in order into one string */
char *
merge_all_in_order( char ** arr, int n )
{
  char * result = strdup( arr[0] );
  for( int i = 1; i < n; i++ ) {
    int ov = compute_overlap( result, arr[i] );
    char * next = merge_two( result, arr[i], ov );
    free( result );
    result = next;
  }
  return result;
}

char *
two_opt_improve( char ** arr, int n )
{
  /* work on a local copy */
  char ** order = malloc( n * sizeof(char *) );
  assert( order != NULL );
  for( int i = 0; i < n; i++ )
    order[i] = strdup( arr[i] );

  int improved = 1;
  while( improved ) {
    improved = 0;
    int current_len = compute_total_length( order, n );

    for( int i = 0; i < n - 1; i++ ) {
      for( int j = i + 1; j < n; j++ ) {

        /* swap order[i] and order[j] */
        char * tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;

        int new_len = compute_total_length( order, n );

        if( new_len < current_len ) {
          current_len = new_len;   /* keep swap */
          improved = 1;
        } else {
          /* swap back */
          tmp = order[i];
          order[i] = order[j];
          order[j] = tmp;
        }
      }
    }
  }

  char * result = merge_all_in_order( order, n );
  for( int i = 0; i < n; i++ ) free( order[i] );
  free( order );
  return result;
}

/* ───────────────────────────────────────────────────────────────
   main: reads fragments, runs greedy + 2-opt, outputs best result
   ─────────────────────────────────────────────────────────────── */

int main( int argc, char * argv[] )
{
  if( argc != 2 ) {
    fprintf( stderr, "Usage: %s [ <input_file> | - ]\n", argv[0] );
    return 1;
  }

  /* load and preprocess fragments */
  struct fragment_s * list = read_all_fragments( argv[1] );
  int n = count_fragments( list );
  char ** arr = list_to_array( list, n );
  free_all_fragments( list );

  /* edge case: no fragments */
  if( n == 0 ) {
    fprintf( stderr, "No fragments found.\n" );
    free( arr );
    return 1;
  }

  /* edge case: single fragment */
  if( n == 1 ) {
    printf( "%s\n", arr[0] );
    free_array( arr, n );
    return 0;
  }

  /* step 1: remove contained fragments */
  int new_n = remove_contained( arr, n );

  /* step 2: greedy ordering */
  char ** ordered = greedy_order( arr, new_n );

  /* step 3: greedy merge for quick result */
  char * greedy_result = greedy_reconstruct( arr, new_n );

  /* step 4: 2-opt improvement */
  char * final_result = two_opt_improve( ordered, new_n );

  /* output the best result found */
  char * best_result;

  if( strlen( greedy_result ) <= strlen( final_result ) )
    best_result = greedy_result;
  else
    best_result = final_result;

  printf( "%s\n", best_result );

  /* cleanup */
  free( greedy_result );
  free( final_result );

  for( int i = 0; i < new_n; i++ )
    free( ordered[i] );

  free( ordered );
  free_array( arr, new_n );

  return 0;
}
