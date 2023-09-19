#include <algorithm>
#include <chrono>
#include <random>
#include <iostream>
#include <climits>
#include <map>
#include <unordered_map>
#include <unordered_set>
using namespace std;

#include "board.hpp"
#include "engine.hpp"

const int SEARCH_DEPTH = 4;

const int STALEMATE_WEIGHT = 1000000;
const int PAWN_WEIGHT = 100;
const int ROOK_WEIGHT = 500;
const int BISHOP_WEIGHT = 300;
const int KING_WEIGHT = 100000000;
const int CHECK_WEIGHT = 500;
const int ATTACKING_FACTOR = 6;
const int DEFENDING_FACTOR = 4;

unordered_map<string, int> mp;


void flip_player(Board& b) {
    b.data.player_to_play = (PlayerColor)(b.data.player_to_play ^ (WHITE | BLACK));
}

int nodes_visited;
int curr_player = -1;

int eval(Board& b) {
    int score = 0;
    U8 white_pieces[6] = {b.data.w_rook_ws, b.data.w_rook_bs, b.data.w_king, b.data.w_bishop, b.data.w_pawn_ws, b.data.w_pawn_bs};
    U8 black_pieces[6] = {b.data.b_rook_ws, b.data.b_rook_bs, b.data.b_king, b.data.b_bishop, b.data.b_pawn_ws, b.data.b_pawn_bs};
    int weights[6] = {ROOK_WEIGHT, ROOK_WEIGHT, KING_WEIGHT, BISHOP_WEIGHT, PAWN_WEIGHT, PAWN_WEIGHT};
    U8 *player_pieces, *opponent_pieces;
    player_pieces = (curr_player == WHITE ? white_pieces : black_pieces);
    opponent_pieces = (curr_player == WHITE ? black_pieces : white_pieces);
    int promo_x = (curr_player == WHITE ? 4 : 2);
    int promo_y = (curr_player == WHITE ? 5 : 0);
    int kx = getx(white_pieces[2]);
    int ky = gety(white_pieces[2]);

    if (b.in_check()) {
        score -= CHECK_WEIGHT;
    }

    for (int i = 0; i < 6; i++) {
        if (player_pieces[i] != DEAD) {
            score += weights[i];

            int tempx,tempy;
            tempx = getx(white_pieces[i]);
            tempy = gety(white_pieces[i]);
            score += weights[i] / (20 + abs(ky - tempy) + abs(kx - tempx));
            if (i >= 4){
                int px, py;
                px = getx(player_pieces[i]);
                py = gety(player_pieces[i]);
                if (min(abs(py - promo_y), abs(py - promo_y - 1)) <= 1){
                    score += 400 / (1 + abs(px - promo_x));
                } else {
                    score += 80 / (1 + abs(px - promo_x) + min(abs(py - promo_y), abs(py - promo_y - 1)));
                }
            }
        }
        if (opponent_pieces[i] != DEAD) {
            score -= weights[i];
        }
    }

    unordered_set<U16> player_moves = b.get_legal_moves();


    for (auto move : player_moves) {
        // Board* new_board = b.copy();
        // new_board->do_move(move);
        // unordered_set<U16> opponent_moves = new_board->get_legal_moves();
        // free(new_board);
        // if (opponent_moves.empty()) {
        //     score += KING_WEIGHT;
        // }
        U8 final_pos = getp1(move);
        for (int i = 0; i < 6; i++) {
            if (final_pos == opponent_pieces[i]) {
                score += weights[i] / ATTACKING_FACTOR;
            }
        }
    }

    flip_player(b);
    unordered_set<U16> opponent_moves = b.get_legal_moves();
    flip_player(b);

    for (auto move : opponent_moves) {
        U8 final_pos = getp1(move);
        for (int i = 0; i < 6; i++) {
            if (final_pos == player_pieces[i]) {
                score -= weights[i] / DEFENDING_FACTOR;
            }
        }
    }


    // add weight of edge control using rooks
    // add position weights

    return score;
}


int minimax(Board& board, int depth, bool maximizing_player, vector<Board*> &visited, int alpha, int beta) {
    if (mp[all_boards_to_str(board)] == 2) {
        return (maximizing_player ? INT_MAX : INT_MIN);
    }
    if (depth == 0) {
        return eval(board);
    }

    int best_eval = (maximizing_player ? INT_MIN : INT_MAX);
    auto player_moveset = board.get_legal_moves();
    if (player_moveset.empty() && !board.in_check() && !maximizing_player) {
        return 0;
    }
    for (auto move : player_moveset) {
        Board* new_board = board.copy();
        new_board->do_move(move);
        nodes_visited++;
        bool is_visited_board = false;
        for (Board* b : visited) {
            if (b->data == new_board->data) {
                is_visited_board = true;
                break;
            }
        }
        if (is_visited_board) {
            free(new_board);
            continue;
        }
        visited.push_back(new_board);
        int eval = minimax(*new_board, depth - 1, !maximizing_player, visited, alpha, beta);
        free(new_board);
        visited.pop_back();
        if (maximizing_player) {
            best_eval = max(best_eval, eval);
            alpha = max(alpha, best_eval);
        } else {
            best_eval = min(best_eval, eval);
            beta = min(beta, best_eval);
        }
        if (alpha >= beta) {
            break;
        }
    }
    return best_eval;
}

void Engine::find_best_move(const Board& b) {
    auto start_time = chrono::high_resolution_clock::now();
    mp[all_boards_to_str(b)]++;
    if (curr_player == -1) {
        curr_player = b.data.player_to_play;
    }
    int best_eval = INT_MIN;
    int best_move = 0;
    auto player_moveset = b.get_legal_moves();
    this->best_move = 0;
    // std::cout << all_boards_to_str(b) << std::endl;
    // for (auto m : moveset) {
    //     std::cout << move_to_str(m) << " ";
    // }
    // std::cout << std::endl;
    vector<Board*> visited;
    nodes_visited = 0;
    for (auto move : player_moveset) {
        // Apply the move to a copy of the board
        Board* new_board = b.copy();
        new_board->do_move(move);
        visited.push_back(new_board);
        nodes_visited++;
        int alpha = INT_MIN;
        int beta = INT_MAX;
        int eval = minimax(*new_board, SEARCH_DEPTH - 1, false, visited, alpha, beta);
        visited.pop_back();
        free(new_board);
        if (eval > best_eval) {
            best_eval = eval;
            best_move = move;
        }
    }
    this->best_move = best_move;
    auto end_time = chrono::high_resolution_clock::now();
    cout << "time taken to find best move: " << std::chrono::duration_cast<std::chrono::duration<double>>(end_time - start_time).count() << " seconds" << endl;
    cout << "nodes visited " << nodes_visited << endl;
}
