#include <algorithm>
#include <chrono>
#include <iostream>
#include <climits>
#include <unordered_map>
#include <unordered_set>

using namespace std;

#include "board.hpp"
#include "engine.hpp"

const int SEARCH_DEPTH = 4;

const int PAWN_WEIGHT = 100;
const int ROOK_WEIGHT = 500;
const int BISHOP_WEIGHT = 300;
const int KING_WEIGHT = 10000;
const int CHECK_WEIGHT = 500;

const int ATTACKING_FACTOR = 6;
const int DEFENDING_FACTOR = 4;

int nodes_visited;
int curr_player = -1;

int WEIGHTS[6] = {ROOK_WEIGHT, ROOK_WEIGHT, KING_WEIGHT, BISHOP_WEIGHT, PAWN_WEIGHT, PAWN_WEIGHT};

unordered_map<string, int> previous_board_occurences;

void flip_player(Board& b) {
    b.data.player_to_play = (PlayerColor)(b.data.player_to_play ^ (WHITE | BLACK));
}

int eval(Board& b) {

    U8 white_pieces[6] = {b.data.w_rook_ws, b.data.w_rook_bs, b.data.w_king, b.data.w_bishop, b.data.w_pawn_ws, b.data.w_pawn_bs};
    U8 black_pieces[6] = {b.data.b_rook_ws, b.data.b_rook_bs, b.data.b_king, b.data.b_bishop, b.data.b_pawn_ws, b.data.b_pawn_bs};

    U8* player_pieces = (curr_player == WHITE ? white_pieces : black_pieces);
    U8* opponent_pieces = (curr_player == WHITE ? black_pieces : white_pieces);

    int player_promo_x = (curr_player == WHITE ? 4 : 2);
    int player_promo_y = (curr_player == WHITE ? 5 : 0);
    int opponent_promo_x = (curr_player == WHITE ? 2 : 4);
    int opponent_promo_y = (curr_player == WHITE ? 0 : 5);

    int player_king_x = getx(player_pieces[2]);
    int player_king_y = gety(player_pieces[2]);
    int opponent_king_x = getx(opponent_pieces[2]);
    int opponent_king_y = gety(opponent_pieces[2]);

    int score = 0;

    unordered_set<U16> player_moves, opponent_moves;
    (curr_player == b.data.player_to_play ? player_moves : opponent_moves) = b.get_legal_moves();
    flip_player(b);
    (curr_player == b.data.player_to_play ? player_moves : opponent_moves) = b.get_legal_moves();
    flip_player(b);

    auto add_piece_score = [&]() {
        for (int i = 0; i < 6; i++) {
            if (player_pieces[i] != DEAD) {
                score += WEIGHTS[i];
            }
            if (opponent_pieces[i] != DEAD) {
                score -= WEIGHTS[i];
            }
        }
    };

    auto add_attack_score = [&]() {
        for (auto move : player_moves) {
            U8 final_pos = getp1(move);
            for (int i = 0; i < 6; i++) {
                if (final_pos == opponent_pieces[i]) {
                    score += WEIGHTS[i] / ATTACKING_FACTOR;
                }
            }
        }
        for (auto move : opponent_moves) {
            U8 final_pos = getp1(move);
            for (int i = 0; i < 6; i++) {
                if (final_pos == player_pieces[i]) {
                    score -= WEIGHTS[i] / DEFENDING_FACTOR;
                }
            }
        }
    };

    auto subtract_check_score = [&]() {
        if (b.in_check()) {
            score -= CHECK_WEIGHT;
        }
    };

    auto add_promo_score = [&]() {
        int piece_x, piece_y, distance_x, distance_y;
        for (int i = 4; i < 6; i++) {
            if (player_pieces[i] == DEAD) {
                continue;
            }
            piece_x = getx(player_pieces[i]);
            piece_y = gety(player_pieces[i]);
            distance_x = abs(piece_x - player_promo_x);
            distance_y = min(abs(piece_y - player_promo_y), abs(piece_y - player_promo_y - 1));
            if (distance_y <= 1){
                score += 400 / (1 + distance_x);
            } else {
                score += 80 / (1 + distance_x + distance_y);
            }
        }
        for (int i = 4; i < 6; i++) {
            if (opponent_pieces[i] == DEAD) {
                continue;
            }
            piece_x = getx(opponent_pieces[i]);
            piece_y = gety(opponent_pieces[i]);
            distance_x = abs(piece_x - opponent_promo_x);
            distance_y = min(abs(piece_y - opponent_promo_y), abs(piece_y - opponent_promo_y - 1));
            if (distance_y <= 1){
                score -= 400 / (1 + distance_x);
            } else {
                score -= 80 / (1 + distance_x + distance_y);
            }
        }
    };

    auto add_king_distance_score = [&]() {
        for (int i = 0; i < 6; i++) {
            if (player_pieces[i] == DEAD) {
                continue;
            }
            int player_piece_x = getx(player_pieces[i]);
            int player_piece_y = gety(player_pieces[i]);
            score += WEIGHTS[i] / (20 + abs(opponent_king_y - player_piece_y) + abs(opponent_king_x - player_piece_x));
        }
        for (int i = 0; i < 6; i++) {
            if (opponent_pieces[i] == DEAD) {
                continue;
            }
            int opponent_piece_x = getx(opponent_pieces[i]);
            int opponent_piece_y = gety(opponent_pieces[i]);
            score -= WEIGHTS[i] / (20 + abs(player_king_y - opponent_piece_y) + abs(player_king_x - opponent_piece_x));
        }
    };

    add_piece_score();
    add_attack_score();
    subtract_check_score();
    add_promo_score();
    add_king_distance_score();


    // add weight of edge control using rooks
    // add position weights

    return score;
}

bool is_equal(Board* b1, Board* b2) {
    bool is_king_equal = (b1->data.b_king == b2->data.b_king) && (b1->data.w_king == b2->data.w_king);
    bool is_rook_ws_equal = (b1->data.b_rook_ws == b2->data.b_rook_ws) && (b1->data.w_rook_ws == b2->data.w_rook_ws);
    bool is_rook_bs_equal = (b1->data.b_rook_bs == b2->data.b_rook_bs) && (b1->data.w_rook_bs == b2->data.w_rook_bs);
    bool is_pawn_ws_equal = (b1->data.b_pawn_ws == b2->data.b_pawn_ws) && (b1->data.w_pawn_ws == b2->data.w_pawn_ws);
    bool is_pawn_bs_equal = (b1->data.b_pawn_bs == b2->data.b_pawn_bs) && (b1->data.w_pawn_bs == b2->data.w_pawn_bs);
    bool is_bishop_equal = (b1->data.b_bishop == b2->data.b_bishop) && (b1->data.w_bishop == b2->data.w_bishop);
    bool is_equal = is_king_equal && is_rook_ws_equal && is_rook_bs_equal && is_pawn_ws_equal && is_pawn_bs_equal && is_bishop_equal;
    return is_equal;
}


int minimax(Board& board, int depth, bool maximizing_player, vector<Board*> &visited, int alpha, int beta) {
    if (previous_board_occurences[all_boards_to_str(board)] == 2) {
        return (maximizing_player ? INT_MAX : INT_MIN);
    }
    if (depth == 0) {
        return eval(board);
    }

    int best_eval = (maximizing_player ? INT_MIN : INT_MAX);
    auto player_moveset = board.get_legal_moves();
    // if (player_moveset.empty() && !board.in_check() && !maximizing_player) {
    //     return 0;
    // }
    for (auto move : player_moveset) {
        Board* new_board = board.copy();
        new_board->do_move(move);
        bool is_visited_board = false;
        for (Board* b : visited) {
            if (is_equal(b, new_board)) {
                is_visited_board = true;
                break;
            }
        }
        if (is_visited_board) {
            free(new_board);
            continue;
        }
        visited.push_back(new_board);
        nodes_visited++;
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
    previous_board_occurences[all_boards_to_str(b)]++;
    if (curr_player == -1) {
        curr_player = b.data.player_to_play;
    }
    int best_eval = INT_MIN;
    U16 best_move = 0;
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
        Board* new_board = b.copy();
        new_board->do_move(move);
        visited.push_back(new_board);
        nodes_visited++;
        int alpha = INT_MIN;
        int beta = INT_MAX;
        int eval = minimax(*new_board, SEARCH_DEPTH - 1, false, visited, alpha, beta);
        visited.pop_back();
        free(new_board);
        if (eval > best_eval || best_eval == INT_MIN) {
            best_eval = eval;
            best_move = move;
        }
    }
    this->best_move = best_move;
    auto end_time = chrono::high_resolution_clock::now();
    cout << "time taken to find best move: " << chrono::duration_cast<chrono::duration<double>>(end_time - start_time).count() << " seconds" << endl;
    cout << "nodes visited " << nodes_visited << endl;
}
